//**************************************************************************/
// DESCRIPTION: Manages the undo system for our Dynamic Attributes, and
//				is responsible for releasing descriptors no longer needed.
// AUTHOR: Stephen Taylor
//***************************************************************************/

#include "StdAfx.h"
#include "DynPBUndo.h"
#include "DynPBCustAttrClassDesc.h"

DynPBUndo::DynPBUndo(ParamBlockDesc2* pbdesc, ReferenceTarget* pDynPB)
:	m_pFabricClient(pDynPB)
,	m_pbOldDesc(pbdesc)
,	m_pDynPBClassDesc(dynamic_cast<DynPBCustAttrClassDesc*>(pbdesc->cd))
,	m_pbNewDesc(NULL)
,	m_isHeld(true)
{
	// Our paramblock is removed from the scene, so now we release
	// the descriptor as well.  This keeps things slightly cleaner,
	// and ensures that the descriptor is not saved wastefully.

	// Lets just verify we have the right objects...
	//DynPBCustAttrClassDesc* ourCD = GetDynPBCustAttrClassDesc();
	DbgAssert(m_pDynPBClassDesc != NULL);

	// Release our descriptor
	if (m_pDynPBClassDesc != NULL)
		m_pDynPBClassDesc->ReleasePBDesc(m_pbOldDesc, FALSE);
}

DynPBUndo::~DynPBUndo() 
{
	// Sanity
	if (m_pDynPBClassDesc == NULL)
		return;

	// When this function is called, we are being pushed off the end of the undo queue.
	// At this point, if we are held, then the paramblock has not been restored to the scene
	// and would have been deleted by now - we are free to delete the descriptor
	if (m_isHeld)
	{
		// We have not been undone - delete the descriptor for the original block
		m_pDynPBClassDesc->ReleasePBDesc(m_pbOldDesc, TRUE);
	}
	else
	{
		// We have been undone.  This means the original block is
		// back in play, and the new one is out.  Unfortunately,
		// because of the order of the undo queue we can't just delete
		// this - the actual block it is part of has not been released yet.
		m_pDynPBClassDesc->SetObsoletePBDesc(m_pbNewDesc);
	}
}

void DynPBUndo::Restore(int isUndo)
{
	// Sanity
	if (m_pDynPBClassDesc == NULL)
		return;

	// if we have been restored, which means that
	// our desc is used again
	DbgAssert(m_isHeld == true);
	m_isHeld = false;

	// Wire the desc back into the CD, to make it available to the scene
	m_pDynPBClassDesc->AddParamBlockDesc(m_pbOldDesc);

	// If this action has been cancelled, we may not have a new descriptor to release
	if (isUndo && m_pbNewDesc != NULL)
	{
		// The new block is in UNDO limbo too.  Release the block, so it
		// does not get save in the scene (but DO NOT DELETE it).
		m_pDynPBClassDesc->ReleasePBDesc( m_pbNewDesc, FALSE );
	}
}

void DynPBUndo::Redo() {
	// we have been re-done, which means
	// that our desc is again free
	DbgAssert(m_isHeld == false);
	m_isHeld = true;

	// Our original desc is back in UNDO limbo.  Release it so it
	// it is not saved with the scene (but DO NOT DELETE it).
	m_pDynPBClassDesc->ReleasePBDesc(m_pbOldDesc, FALSE);
	// The new block is back in use.  Add our new descriptor back to 
	// the ClassDesc, to allow it to be saved with the scene.
	m_pDynPBClassDesc->AddParamBlockDesc(m_pbNewDesc);
}

void DynPBUndo::EndHold() {
	// This is called when the undo operation is complete.
	// Now, if we have been accepted, there is a new
	// block in residence on the cust attrib.
	IParamBlock2* newPB = m_pFabricClient->GetParamBlock(0);
	// We will need this new descriptor later if we undo/redo.
	if (newPB != NULL) m_pbNewDesc = newPB->GetDesc();

	DbgAssert(m_pbNewDesc != m_pbOldDesc);
}
