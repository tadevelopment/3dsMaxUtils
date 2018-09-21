//**************************************************************************/
// DESCRIPTION: 
//	The ClassDesc2 class manages the ParameterBlock descriptions, as well
//	as describing the DynPBCustAttrib class to Max
// AUTHOR: 
//	Stephen Taylor
//***************************************************************************/

#pragma once

//////////////////////////////////////////////////////////////////////////
// Utility Fns

//--- DynPBCustAttrClassDesc -------------------------------------------------------

class DynPBCustAttrClassDesc : public ClassDesc2 
{
private:
	// Private variables
	UINT16					m_freeBlockId;		// Track the highest block ID currently in use
	Tab<ParamBlockDesc2*>	m_dObsoleteDescs;	// Stores any descs scheduled for later deletion

public:

	DynPBCustAttrClassDesc();
	~DynPBCustAttrClassDesc();

	// The standard implementations of Class Desc fn's
	int 			IsPublic()						{ return TRUE;}
	const MCHAR* 	Category()						{ return GetString(IDS_CATEGORY); }

	HINSTANCE		HInstance()						{ return MaxSDK::GetHInstance(); }			// returns owning module handle


	/*! Implement this function to return TRUE if we need to save anything */
	BOOL			NeedsToSave()					{ return TRUE; }
	/*! Save our current list of param block descriptors */
	IOResult		Save(ISave *isave);
	/*! Load a list of paramblock descriptors for the new scene */
	IOResult		Load(ILoad *iload);
	/*! Load a single descriptor */
	IOResult		LoadParamBlockDesc2( ILoad* iload );

	/*! Return a block ID.  This is guaranteed to be unused (unique) in the scene. */
	UINT16 GetFreeParamBlockId()					{ return m_freeBlockId++; }
	/*! Register an ID as being used.  It is guaranteed to be unused on completion of the fn */
	void SetTakenId(UINT16 id);

	/*! Create a new descriptor, with the default name and flags. 
		\param blockID - The id of the new parameter descriptor.  If -1, assign the next free id 
		\return A default empty parameter descriptor */
	ParamBlockDesc2* CreatePBDesc(BlockID blockID=-1);
	/*! Release the descriptor from lists registered with this ClassDesc2. 
		\param pbDesc - the descriptor to remove
		\param doDelete - If false, pbDesc will simply be removed from this ClassDescs 
			list of active parameter block descriptors.  If true, pbDesc will be 
			delete'd as well.  */
	void ReleasePBDesc(ParamBlockDesc2* pbDesc, BOOL doDelete=TRUE);
	/*! Set the descriptor as obsolete.  This will cause the descriptor to be deleted later.
		\param pbDesc - The descriptor to release. This descriptor should no longer
			be used anywhere, or be in the process of being released. */
	void SetObsoletePBDesc(ParamBlockDesc2* pbDesc);
	/*! Release any descriptors scheduled for deletion via SetObsoletePBDesc */
	void ReleaseObsoletePBDesc();

	/*! Allow us to register IParamMaps (necessary for creating our own ParamDlg for materials */
	void RegisterMParamMap(IParamMap2* pNewDlg) { GetParamMaps().Append(1, &pNewDlg); }

};
