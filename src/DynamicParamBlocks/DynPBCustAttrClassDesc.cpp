//**************************************************************************/
// DESCRIPTION: 
//	The ClassDesc2 class manages the ParameterBlock descriptions, as well
//	as describing the DynPBCustAttrib class to Max
// AUTHOR: 
//	Stephen Taylor
//***************************************************************************/

#include "StdAfx.h"
//#include "DynPBCustAttr.h"
#include "DynPBCustAttrClassDesc.h"

//////////////////////////////////////////////////////////////////////////
//--- DynPBCustAttrClassDesc -------------------------------------------------

//---------------------------------------------------------------------------------------------------------------
// Constructor/Destructor
//---------------------------------------------------------------------------------------------------------------
#pragma region//Constructor/Destructor
DynPBCustAttrClassDesc::DynPBCustAttrClassDesc()
{
}

DynPBCustAttrClassDesc::~DynPBCustAttrClassDesc()
{
	// Triple check we have cleaned everything up!
	assert(NumParamBlockDescs() == 0);
}
#pragma endregion

//---------------------------------------------------------------------------------------------------------------
// Standard ClassDesc2 fn implementations
//---------------------------------------------------------------------------------------------------------------
#pragma region//Create/ClassID

#pragma endregion

//---------------------------------------------------------------------------------------------------------------
// Serialization
//---------------------------------------------------------------------------------------------------------------
#pragma region//Save/Load ParamBlockDesc2
#define PB_DESC_CHUNK		1 << 1
#define PB_DESC_HDR_CHUNK	1 << 2
#define PB_DESC_PARAM_CHUNK	1 << 3

// Load our data
IOResult DynPBCustAttrClassDesc::Load( ILoad *iload )
{
	IOResult res;
	while (IO_OK==(res=iload->OpenChunk()))
	{
		switch (iload->CurChunkID())
		{
		case PB_DESC_CHUNK:
			res = LoadParamBlockDesc2( iload );
			break;
		}
		iload->CloseChunk();
		if (res!=IO_OK) 
			return res;
	}

	return IO_OK;
}

// Save our parameter block descriptors
IOResult DynPBCustAttrClassDesc::Save( ISave *isave )
{
	// With static blocks, there is no need to save the descriptors.
	// They are defined at compile time so we know they are not going to
	// change between Max sessions,
	// ... We are not so lucky.  We need to save all our descriptors that
	// are currently being used so next time the scene is loaded, the 
	// parameter blocks will be able to find their descriptors to load.
	ULONG written;
	IOResult res = IO_OK;

	// iterate over descs
	const int num = NumParamBlockDescs();
	for ( int i=0; i<num; ++i )
	{
		// get desc
		ParamBlockDesc2* pDesc = GetParamBlockDesc( i );
		if ( !pDesc )
			continue;

		// open chunk for this desc
		isave->BeginChunk( PB_DESC_CHUNK );

		// Write the main descriptor stuff
		// What data do we need to save here?  
		//
		// What we need here is to focus on the information 
		// that is necessary to recreate the block.
		// In other words - only the information that varies from
		// block to block...

		isave->BeginChunk( PB_DESC_HDR_CHUNK );
		// Save descriptor ID
		res = isave->Write( &pDesc->ID, sizeof pDesc->ID, &written );		if (res!=IO_OK) return res;
		isave->EndChunk();			// PB_DESC_HDR_CHUNK

		// iterate over parameters
		for ( UINT j=0; j<pDesc->Count(); ++j )
		{
			// Write out each parameter
			// Here we write out the minimum data needed to recreate the parameter.
			// We can cheat here in several pretty definite ways...  We do not need
			// to save any UI information (it is generated as required)
			// Neither do we need to save any information that is not explicitly set
			// by us (for example, the range values, scale, etc)
			// We do have to careful to save the ID - because we calculate it,
			// and it _must_ be consistent between saves...

			ParamID paramId = pDesc->IndextoID(j);
			ParamDef& def = pDesc->GetParamDef( paramId );
			isave->BeginChunk( PB_DESC_PARAM_CHUNK );
			// Write out our parameter
			res = isave->Write( &def.type, sizeof def.type, &written );		if (res!=IO_OK) return res;
			res = isave->Write( &def.ID, sizeof def.ID, &written );			if (res!=IO_OK) return res;
			isave->EndChunk();			// PB_DESC_PARAM_CHUNK
		}
		
		isave->EndChunk();			// PB_DESC_CHUNK
	}

	return IO_OK;
}

// Load one ParamBlockDesc2.
IOResult DynPBCustAttrClassDesc::LoadParamBlockDesc2( ILoad* iload )
{
	ULONG read;
	IOResult res;
	ParamBlockDesc2* pDesc = NULL;

	while (IO_OK==(res=iload->OpenChunk()))
	{
		switch (iload->CurChunkID())
		{
		case PB_DESC_HDR_CHUNK:
			{
				// don't load header 2x times
				if ( pDesc ) return IO_ERROR;

				// read data
				BlockID id;
				res = iload->Read( &id, sizeof id, &read );						if (res!=IO_OK) return res;

				// We are loading a parameter descriptor with a set ID.  We cannot change the
				// id on our new descriptor, because this is how the parameter block (which loads later)
				// finds the appropriate descriptor.
				pDesc = CreatePBDesc(id);
			}
			break;
		case PB_DESC_PARAM_CHUNK:
			{
				// load parameter
				if ( !pDesc ) return IO_ERROR;

				// Our saved information
				ParamType2 type;
				ParamID pid;

				// From this data we recreate everything we need for the parameter.
				// Note that this does NOT include any UI data - we apply that dynamically
				// when generating the UI templates in DynPBCustAttrib
				res = iload->Read( &type, sizeof type, &read );							if (res!=IO_OK) return res;
				res = iload->Read( &pid, sizeof pid, &read );							if (res!=IO_OK) return res;

				// We now have all the required information to create the parameter.
				ParamID tmpid = AddMaxParameter(pDesc, type, "TODO");
				if (tmpid == -1)
				{
					//ImplementMe, we loaded an invalid type
					DbgAssert(FALSE);
					iload->CloseChunk();
					return IO_ERROR;
				}

				// If our new ID doesn't match the loaded one...
				if (tmpid != pid)
				{
					// Enforce our previous ID's!
					pDesc->GetParamDef(tmpid).ID = pid;
				}
			}
			break;
		}
		iload->CloseChunk();
		if (res!=IO_OK) 
			return res;
	}

	// We must have loaded _something_
	if ( !pDesc )
		return IO_ERROR;

	return IO_OK;
}
#pragma endregion

//---------------------------------------------------------------------------------------------------------------
// Paramblock ID management
//---------------------------------------------------------------------------------------------------------------
#pragma region// Reserve (Set Taken) ID's

/*! Class FreeBlockIDEnum
	A parameter block ID is stored purely on the blocks
	descriptor.  This class can be used to iterate all
	blocks in the scene (including those in the undo queue)
	and change the id of the existing descriptor so it is
	unique when assigned to a new descriptor */
class FreeBlockIDEnum : public Animatable::EnumAnimList {
private:
	UINT16 m_id;
	DynPBCustAttrClassDesc* m_pMyDesc;
public:
	FreeBlockIDEnum(UINT16 id, DynPBCustAttrClassDesc* pMyDesc) 
		: m_id(id)
		, m_pMyDesc(pMyDesc) 
	{}

	bool proc (Animatable *theAnim) 
	{
		// If theAnim is actually a parameter
		// block, which is one of ours, we will
		// need to ensure that it's ID does
		// not collide with our set ID
		if (theAnim->SuperClassID() == PARAMETER_BLOCK2_CLASS_ID)
		{
			IParamBlock2* pblock = (IParamBlock2*)theAnim;
			ParamBlockDesc2* pDesc = pblock->GetDesc();
			// Is this one of ours?
			if (pDesc->cd == m_pMyDesc)
			{
				// Does it collide?
				if (pDesc->ID == m_id)
				{
					// Replace with free id, (then return, we can only
					// have 1 paramblock per ID).
					pDesc->ID = m_pMyDesc->GetFreeParamBlockId();
					return false;
				}
			}
		}
		return true;
	}
};

void DynPBCustAttrClassDesc::SetTakenId(UINT16 id) 
{
	// ensure that nobody else uses this id.
	if (m_freeBlockId <= id) m_freeBlockId = id+1;
	else 
	{
		// How do we free an id?  We must find any other pblocks
		// whose ID's collide, and re-assign them!
		// If we are being XRef'ed or merged, then it is
		// possible that a parameter block of ours already in the
		// scene uses this ID.  Because the loading depends on the
		// id, we cannot simply change the id - if we do this
		// the parameter block will not find the correct descriptor
		// on load.  We need to free up the id from any other
		// parameter blocks in the scene.
		//
		// We can do this by changing the ID of any existing blocks.
		// There is no penalty for this, the ID is generally only used
		// in load operations.
		FreeBlockIDEnum enumerator(id, this);
		Animatable::EnumerateAllAnimatables(enumerator);
	}
}

#pragma endregion

//---------------------------------------------------------------------------------------------------------------
// Paramblock Descriptor management.
//---------------------------------------------------------------------------------------------------------------
#pragma region//Create/Release ParamBlockDesc
ParamBlockDesc2* DynPBCustAttrClassDesc::CreatePBDesc( BlockID blockID /*!=-1*/ )
{
	// Ensure that this ID is unique.  If -1, get the next free one.
	// Otherwise, force any existing parameter blocks with this ID
	// to free it (existing blocks can take a new ID, but we can't)
	if (blockID == -1) 
	{
		blockID = GetFreeParamBlockId();
	}
	else
	{
		// Ensure that we don't duplicate this ID anywhere.
		SetTakenId(blockID);
	}

	// Create a new default parameter block and return it.
	// Each descriptor should have a unique ID.
	USHORT	flags = P_TEMPLATE_UI;

	// return a new descriptor
	return new ParamBlockDesc2(blockID, _T("DynamicPB"), 0, this, flags, 
		p_end);
}

void DynPBCustAttrClassDesc::ReleasePBDesc( ParamBlockDesc2* pbDesc, BOOL doDelete/*=TRUE*/ )
{
	// Its possible we may be called with a NULL desc
	// if an undo object is released that was not 
	// completed (ie - action was canceled before
	// completion, or some other issue caused the action to fail
	if (pbDesc == NULL)
		return;
	
	// Our ClassDescriptor contains pointers to all the
	// parameter block descriptors created.  If we want
	// to release the pblock descriptor, we need to remove
	// the pointer from this list, because it wont happen
	// automatically (and we get left with bad pointers).
	// 
	// There is no built-in functionality for removing a
	// paramblockdesc from a ClassDesc.  
	// Although this is not a supported operation, clearing
	// all descriptors is.  So we clear all, and add back 
	// in the descriptors not being released.
	int pbCount = NumParamBlockDescs();
	std::vector<ParamBlockDesc2*> allPblockDescs;
	allPblockDescs.reserve(pbCount);
	for( int i=0; i < pbCount; ++i )
	{
		ParamBlockDesc2* pd = GetParamBlockDesc(i);
		if (pd != pbDesc) allPblockDescs.push_back(pd);
	}
	// Remove all descriptors
	ClearParamBlockDescs();
	for( size_t i=0; i < (size_t)allPblockDescs.size(); ++i )
	{
		AddParamBlockDesc( allPblockDescs[i] );
	}

	// If we can, delete this.
	if (doDelete) 
	{
		// We need to manually free the UI strings
		// when deleting parameter blocks
		for (int i = 0; i < pbDesc->Count(); i++)
		{
			free((void*)pbDesc->paramdefs[i].int_name);
			pbDesc->paramdefs[i].int_name = NULL;
		}
		SAFE_DELETE( pbDesc );
	}
}

void DynPBCustAttrClassDesc::SetObsoletePBDesc(ParamBlockDesc2* pbDesc)
{
	// An obsolete PBDesc is one that is no longer being used,
	// but is not ready for deletion yet - it is in the midst
	// of a ReplaceReference and cannot be released just yet.
	// Save the pointer to this, and delete it later.
	m_dObsoleteDescs.Append(1, &pbDesc);
	ReleasePBDesc(pbDesc, FALSE);
}

void DynPBCustAttrClassDesc::ReleaseObsoletePBDesc()
{
	// Run through the obsolete PBDescs, and do
	// the final hard delete.
	for (int i = 0; i < m_dObsoleteDescs.Count(); i++)
	{
		ReleasePBDesc(m_dObsoleteDescs[i]);
	}
	m_dObsoleteDescs.ZeroCount();
}

#pragma endregion
