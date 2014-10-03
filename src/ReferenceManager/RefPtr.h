//
// Copyright 2009 Autodesk, Inc.  All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license
// agreement provided at the time of installation or download, or which
// otherwise accompanies this software in either electronic or hard copy form.  
//

#pragma once

#include "ReferenceManager.h"

//=========================================================
/// \brief This class provides an easy interface for defining and accessing the references stored on an IReferenceManager.
/// A RefPtr class behaves as a smart pointers, allowing native pointer-like access to references
/// stored and managed by the IReferenceManager implementation in a safe manner.
/// Developers may assign, access, and read a RefPtr as a naked pointer,
/// internally the RefPtr and IReferenceManager ensure that the reference
/// is maintained in accordance to the reference system guidelines.
/// This class is designed to take full advantage of compiler safety - 
/// In general terms - if it compiles, the RefPtr will be valid.
///
/// The sole exception to this rule is the BASE_ID.  This ID needs
/// to be unique in the owning IReferenceManager instance for the lifetime
/// of the RefPtr.  It can only be checked at run-time and will fire a 
/// DbgAssert on construction if the ID is already in use.
/// The BASE_ID also should not change between previously saved
/// versions of max, to ensure that the reference hierarchy on load
/// is equivalent to the hierarchy previously saved.
///
/// An example usage defining an INode reference and a Control reference is 
/// on a ReferenceMaker implementation is below.
/// \code
///	
/// // ReferenceManager implements IReferenceManager, and derives from ReferenceMaker
/// class MyReferenceMaker : public ReferenceManager<ReferenceMaker> {
///		enum RefID {
///			INODE_REF,
///			CTRL_REF,
///		};
///		RefPtr<INode, INODE_REF> m_pINode;
///		RefPtr<Control, CTRL_REF> m_pCtrl;
///		
///		MyReferenceMaker() 
///			: m_pINode(GetRefMgr())
///			, m_pCtrl(GetRefMgr())
///		{
///			// The RefPtr supports assignment to and from raw pointers,
///			// automatically managing the reference in the background.
///			m_pCtrl = NewDefaultFloatController();
///			m_pCtrl->SetValue(0, 1.0f, TRUE, CTRL_ABSOLUTE);
///		}
///	}
/// \endcode
/// \sa IReferenceManager, ReferenceManager, RefArray
/// \param REF_TYPE_T - The type of the reference held, eg ReferenceTarget, INode, etc
/// \param BASE_ID - The ID of the reference group this reference belongs to.  If a class has
///				only static reference pointers (no arrays), this will be equal to the 
///				references index in GetReference.  As such, it is not legal to define
///				multiple RefPtrs with the same ID, doing so will trigger a runtime Assert.
///				Please see IReferenceManager::RegisterReference for more details.
template<typename REF_TYPE_T, int BASE_ID>
class RefPtr {
protected:
	IReferenceManager* m_pMgr;			// The manager who holds our reference
	IReferenceManager::RefInfo* m_ref;	// We hold the pointer directly to Managers RefInfo
										// This allows the underlying RefIDX to change

	// !No default construction!
	RefPtr();
	RefPtr(const RefPtr& rhs);
	//REF_TYPE_T* operator=(RefPtr& rhs);
public:

	/**  Construct a RefPtr, registering the reference with the owning manager.
	This constructor is responsible for ensuring that this RefPtr is fully initialized
	and ready to go.  You may optionally specify an index (if this RefPtr is part of an array)
	and an initial class to reference.
	\param mgr - This should be a (C++) reference to the owning class.
	\param index - IFF this RefPtr is part of an array, this should be the index of this
					parameter in that array.  Normally, developers shouldn't create arrays
					of RefPtr, they should use the RefArray class to manage dynamic 
					arrays of references.
	\param pTarget - An initial reference to target. */
	RefPtr(IReferenceManager& mgr, NotifyCallback* callback=NULL, int index=0, REF_TYPE_T* pTarget = NULL)
		:	m_pMgr(&mgr)
		,	m_ref(mgr.RegisterReference(BASE_ID, index, callback, pTarget))
	{
		// Double check stuff
		DbgAssert(m_ref != NULL);
		DbgAssert(m_ref->m_target == pTarget);
	}

	/** Release the Reference, release the backing ReferenceManager structure. */
	virtual ~RefPtr() {
		// These actions are not undoable
		HoldSuspend hs;
		// This actually resizes the backing RefItem
		m_pMgr->ReleaseReference(m_ref, BASE_ID);
		m_ref = NULL;
	}

	/** Return the pointer referenced by this RefPtr
	\return The pointer referenced */
	REF_TYPE_T* GetRef() { return *this; }

	/** Return the pointer referenced by this RefPtr
	\return The pointer referenced */
	const REF_TYPE_T* GetRef() const { return *this; }

	//----------------------------------------------------
#pragma  region // operator overloads

	/**  Assign a new reference.  
	Releases the current reference, and sets the reference to RHS.  This will DbgAssert
	if rhs is NOT of type REF_TYPE_T.
	\param rhs The new value of the of the reference
	\return The new value of the reference.  
			This will be NULL if rhs cannot be cast to type REF_TYPE_T */
	const REF_TYPE_T* operator=(ReferenceTarget* rhs) {
		// We only set the appropriate type
		REF_TYPE_T* pType = dynamic_cast<REF_TYPE_T*>(rhs); 
		// Check that our incoming value is appropriate
		DbgAssert(pType == rhs);
		// Set the reference
		int n = m_pMgr->GetReferenceIndex(m_ref);
		DbgAssert(n >= 0);
		if (n >= 0)
			m_pMgr->SetRef(n, pType);
		// double check that we have been assigned correctly.
		DbgAssert(pType == m_ref->m_target);
		// Return our type'd pointer
		return pType;
	}

	/** Assign a new reference.  
	Releases the current reference, and sets the reference to the reference pointer contained by RHS.
	Note - this will not reassign the Manager of this RefPtr, just the actual reference pointer.
	\param rhs The new RefPtr to copy the reference from
	\return The new value of the reference.  */
	const REF_TYPE_T* operator=(const RefPtr<REF_TYPE_T, BASE_ID>& rhs) {
		// make the assignment via the equals operator overload (cast to ReferenceTarget
		*this = const_cast<REF_TYPE_T*>(rhs.GetRef());
		return *this;
	}

	/** Allows directly calling underlying pointers.
	Will DbgAssert if we do not currently have a reference **/
	REF_TYPE_T* operator->() {
		DbgAssert(*this != NULL);
		return *this;
	}
	//const REF_TYPE_T* operator->() const {
	//	DbgAssert(*this != NULL);
	//	return *this;
	//}

	/** Allows automatic non-consting of the underlying variable.  This is useful
		when calling IParamBlock2::GetValue with const pointers */
	REF_TYPE_T* operator->() const {
		DbgAssert(dynamic_cast<const REF_TYPE_T*>(GetRef()) != NULL);
		return const_cast<REF_TYPE_T*>(GetRef());
	}
	
	/** Allow direct cast to type
	This function allows us to auto-cast to REF_TYPE_T and use as below
	\code
	REF_TYPE_T* x = RefPtr<REF_TYPE_T> m;
	\endcode */
	operator REF_TYPE_T*() {
		// We know this static_cast is safe, because our assignment
		// operator will validate any value going in (so
		// we do not allow any other types to be set)
		return static_cast<REF_TYPE_T*>(m_ref->m_target);
	}
	/** Allow direct cast to type
	This function allows us to auto-cast to const REF_TYPE_T and use as below
	\code
	const REF_TYPE_T* x = RefPtr<REF_TYPE_T> m;
	\endcode */
	operator const REF_TYPE_T*() const {
		return static_cast<const REF_TYPE_T*>(m_ref->m_target);
	}
#pragma endregion // operator overloads
};


template<typename REF_TYPE_T, int BASE_ID>
class WeakRefPtr : public RefPtr<REF_TYPE_T, BASE_ID>
{
public:
	WeakRefPtr(IReferenceManager& mgr, NotifyCallback* callback=NULL, int index=0, REF_TYPE_T* pTarget = NULL)
		:	RefPtr(mgr, callback, index, pTarget)
	{
		// Assume m_ref has been created successfully
		m_ref->SetIsWeak(true);
	}

	/** Assign a new reference.  
	Releases the current reference, and sets the reference to the reference pointer contained by RHS.
	\param rhs The new RefPtr to copy the reference from
	\return The new value of the reference.  */
	const REF_TYPE_T* operator=(REF_TYPE_T* rhs) {
		// make the assignment via the equals operator overload (cast to ReferenceTarget
		return RefPtr::operator=(rhs);
	}
};

/// \brief RefArray is a dynamically sized array of references, using the RefPtr implementation.  
/// This class should always be preferred when a user wishes to change the number
/// of references over the lifetime of an object.  Its ID should be unique in the owning class.
/// \n Note: When using this class, due to the underlying implementation
/// it is mildly more memory efficient (but not mandatory) to define
/// any RefArrays with a higher index than the static RefPtrs.
/// A sample implementation of a ReferenceMaker with a single RefPtr and a run-time sized 
/// RefArray of INode references is below
/// \code
///	
/// // ReferenceManager implements IReferenceManager, and derives from ReferenceMaker
/// class MyReferenceMaker : public ReferenceManager<ReferenceMaker> {
///		enum RefID {
///			CTRL_REF,
///			INODE_TAB_REF,
///		};
///		RefPtr<Control, CTRL_REF> m_pCtrl;
///		RefArray<INode, INODE_TAB_REF> m_pINodeTab;
///		
///		MyReferenceMaker() 
///			: m_pINodeTab(GetRefMgr())
///			, m_pCtrl(GetRefMgr())
///		{
///			// It is legal to treat our RefPtr as a normal pointer
///			m_pCtrl = NewDefaultFloatController();
///			m_pCtrl->SetValue(0, 1.0f, TRUE, CTRL_ABSOLUTE);
///			
///			// Assign Count new references
///			int count = GetCOREInterace()->GetSelNodeCount()
///			m_pINodeTab.Resize(count);
///			for (int i = 0; i < count; i++)
///				m_pINodeTab[i] = GetCOREInterface()->GetSelNode(i);
///		}
///
///		~MyReferenceMaker()
///		{
///			// No need to release the references, that is handled automatically.
///		}
///	}
/// \endcode
/// \sa IReferenceManager, RefPtr
/// \param REF_TYPE_T The type of the pointers to be referenced in this array
/// \param BASE_ID  The ID of the reference group managed by this RefArray.  All
///					references held in this group will be stored sequentially by
///					ReferenceManager.  It is NOT supported to define multiple arrays
///					with the same ID, and doing so will trigger a run time Assert
///					on construction.
template<typename REF_TYPE_T, int BASE_ID>
class RefArray : public Tab < RefPtr <REF_TYPE_T, BASE_ID> > {
private:
	IReferenceManager* m_pMgr;
	NotifyCallback* m_callback;

	// No default construction
	RefArray();
	RefArray(const RefArray&);

	// None of this either
	Tab& operator=(const Tab& tb);
public:

	/** Contructs the Array, and ensures it is valid.
	\param mgr The owner of this array */
	RefArray(IReferenceManager& mgr, NotifyCallback* callback=NULL)
		: m_pMgr(&mgr), m_callback(callback)
	{
		m_pMgr->RegisterReferenceArray(BASE_ID);
	}

	/** Destruct the array, clean up all references, and release
	the underlying ReferenceManager pointers. */
	~RefArray()
	{
		SetCount(0);
		delete m_callback;
	}

	/** Append a single new reference */
	void Append(REF_TYPE_T* pTarget)
	{
		Append(1, &pTarget);
	}

	/** Append 'n' new references from the pTarget array
	Re-implements the Tab function. See Tab::Append for more docs */
	void Append(int n, REF_TYPE_T** pTarget) 
	{
		int arrayOldSize = Count();
		// Allocate (unconstructed) array
		Tab::SetCount(arrayOldSize + n);
		for (int i = 0; i < n; i++)
		{
			// Call our constructor! (pMgr, idx, target)
			new(Addr(arrayOldSize + i)) RefPtr<REF_TYPE_T, BASE_ID>(*m_pMgr, new NotifyCallback(m_callback), arrayOldSize + i, pTarget[i]);
		}
		// Update the _used_ part of our array
		
	}

	/** Insert 'count' new references from the pTarget array at 'index'
	Re-implements the Tab function. See Tab::Insert for more docs */
	void Insert(int index, int count, REF_TYPE_T** pTarget) 
	{
		int arrayOldSize = Count();
		// Allocate (unconstructed) array
		Tab::Resize(arrayOldSize + count);
		// Move the existing items
		memmove(Addr(index), Addr(index + count), count * sizeof(RefPtr<REF_TYPE_T, BASE_ID>));
		// Call the constructor for new items!
		for (int i = 0; i < count; i++)
			new(Addr(arrayOldSize + i)) RefPtr<REF_TYPE_T, BASE_ID>(*m_pMgr, new NotifyCallback(m_callback), arrayOldSize + i, pTarget[i]);
		// Update the _used_ part of our array
		Tab::SetCount(arrayOldSize + count);
	}

	/** Sets the size of the array to 'n'
	Re-implements the Tab function. See Tab::SetCount for more docs */
	void SetCount(int n) 
	{
		REF_TYPE_T* pNull = NULL;
		// Grow
		while (Count() < n)
		{
			Append(1, &pNull);
		}
		// Shrink
		if (n < Count())
		{
			Delete(n, Count() - n);
		}
	}

	/** Sets the size of the array to 'n'
	Just calls SetCount.  We cannot have un-initialized RefPtrs in our array. */
	void Resize(int n) {
		SetCount(n);
	}

	/** Deletes 'num' references, starting at 'start'
	See Tab::Delete for more docs */
	int Delete(int start, int num) { 
		if (start < 0)
			start = 0;

		int oldCount = Count();
		int numToDelete = min(oldCount-start, num);
		if (numToDelete <= 0)
			return oldCount;

		// Destruct entities
		int maxIdx = start + numToDelete;
		for (int i = maxIdx-1; i >= start; --i)
		{
			(*this)[i].~RefPtr<REF_TYPE_T, BASE_ID>();
		}

		// If necessary, remove remaining items down
		int newCount = oldCount - numToDelete;
		
		if (maxIdx < oldCount)
			memmove(Addr(start), Addr(maxIdx), numToDelete * sizeof(RefPtr<REF_TYPE_T, BASE_ID>));

		Tab::SetCount(newCount);
		return newCount;
	}

	/** Allow directly converting to a Tab of naked pointers
	This is function is provided to make converting old projects a little easier.  
	It is not advised to use this function - its a better idea to maintain references by converting
	to another Tab array.  This function may be deprecated fairly soon. */
	Tab<REF_TYPE_T*> ToTabArray() 
	{
		Tab<REF_TYPE_T*> val;
		// Copy our pointers into val
		val.SetCount(Count());
		for (int i = 0; i < Count(); i++)
			val[i] = (*this)[i];

		return val;
	}

	/** Allow directly converting from a Tab of naked pointers
	This is function is provided to make converting old projects a little easier.  
	It is not advised to use this function - its a better idea to maintain references by converting
	to another Tab array.  This function may be deprecated fairly soon. */
	Tab<REF_TYPE_T*>& FromTabArray(Tab<REF_TYPE_T*>& rhs) 
	{
		// Set to match the number of items
		SetCount(rhs.Count());

		// Copy the references over.
		for (int i = 0; i < Count(); i++)
			(*this)[i] = rhs[i];
		return rhs;
	}

	/** Allow directly converting from a Tab of naked pointers
	This is function is provided to make converting old projects a little easier.  
	It is not advised to use this function - its a better idea to maintain references by converting
	to another Tab array.  This function may be deprecated fairly soon. */
	inline operator Tab<REF_TYPE_T*>() {
		return ToTabArray();
	}

	/** Allow directly converting to a Tab of naked pointers
	This is function is provided to make converting old projects a little easier.  
	It is not advised to use this function - its a better idea to maintain references by converting
	to another Tab array.  This function may be deprecated fairly soon. */
	inline Tab<REF_TYPE_T*>& operator=(Tab<REF_TYPE_T*>& rhs) {
		return FromTabArray(rhs);
	}
};
