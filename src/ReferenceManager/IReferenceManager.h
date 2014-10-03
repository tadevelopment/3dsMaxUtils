//
// Copyright 2009 Autodesk, Inc.  All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license
// agreement provided at the time of installation or download, or which
// otherwise accompanies this software in either electronic or hard copy form.  
//

#pragma once
#include "ref.h"
class RefInfo;

// For a callback, we use the fast-delegate template developed by Don Clugston
// http://www.codeproject.com/Articles/7150/Member-Function-Pointers-and-the-Fastest-Possible
#include "FastDelegate.h"

// Clients may supply an (optional) callback to receive reference messages
//typedef RefResult (NotifyCallback*)(RefMessage, PartID&);
typedef fastdelegate::FastDelegate2<RefMessage, PartID&, RefResult> NotifyCallback;

template <class X, class Y, class Param1, class Param2, class RetType>
NotifyCallback* MakeNotifyCallback(Y* x, RetType (X::*func)(Param1 p1, Param2 p2)) { 
	return new fastdelegate::FastDelegate2<Param1, Param2, RetType>(x, func);
}

//=========================================================
/// This class provides a template-free interface from RefPtr to ReferenceManager
/// Developers should not derive directly from this
/// class.  Instead, derive from ReferenceManager as this
/// class contains implementations for all the functions defined here.
///
/// This class describes an interface for managing arbitrary numbers of 
/// reference targets. 
/// All references should be registered during construction. 
/// Instead of managing this class directly, it is recommended to instead use
/// the RefPtr templates.  The RefPtr automatically hooks into this class to
/// automate reference management, while allowing the user to treat the reference
/// as a native pointer.
class IReferenceManager 
{
protected:

	// All protected functions are allowed from RefPtr/RefArray
	template<typename REF_TYPE_T, int ID> friend class RefPtr;
	template<typename REF_TYPE_T, int ID> friend class RefArray;

	//---------------------------------------------------------------------
	// Stores information about each reference managed by ReferenceManager
	// No other class in Max has access to this class.
#pragma region // RefInfo internal class

#ifndef DOXYGEN_SHOULD_SKIP_THIS	
	struct RefInfo
	{
	private:
		RefInfo(const RefInfo& ); // No copy constructor!

	public:
		enum kRefFlags // describes the state of this reference
		{
			kIsWeak			= 1 << 0,	// Weak Reference
			kIsPersisted	= 1 << 1,	// Is the reference saved/loaded?
			kNumFlags					// Once released, we cannot assign/read this reference
		};

		DWORD m_flags;				// stores our current state
		ReferenceTarget* m_target;	// Stores our current pointer.
		NotifyCallback* m_callback; // A callback for the client to recieve reference messages

		// Flag write access is private
		void SetFlag(kRefFlags flag)	{ m_flags |= flag; }
		void ClearFlag(kRefFlags flag)	{ m_flags = m_flags&~flag; }
		bool TestFlag(kRefFlags flag)	{ return (m_flags&flag) != 0; }

		RefInfo() 
			: m_target(NULL), m_flags(0), m_callback(NULL)
		{ }

		// The standard constructor.  When creating a RefInfo, this
		// is the constructor that is usually used.
		RefInfo(ReferenceTarget* target, NotifyCallback* callback, int flags)
			: m_target(target), m_callback(callback), m_flags(flags)
		{ }

		// We own our m_callback member.  If we are deleted, delete it too
		~RefInfo() { delete m_callback; }

		// Provide accessors for these functions to enforce we do not modify data on a released class
		void SetIsPersisted(bool v) { (v) ? SetFlag(kIsPersisted) : ClearFlag(kIsPersisted); }
		void SetIsWeak(bool v) { (v) ? SetFlag(kIsWeak) : ClearFlag(kIsWeak); }
	};
#endif
#pragma endregion // RefInfo class 

	//========================================================================
	// New reference related methods
	// These functions are _only_ callable from the RefPtr classes.

	/** Allow 'Get' access to the references through this interface
	\param n - The index of the parameter to return.
	If n is invalid (out of range, or a released reference), we assert and
	return NULL. */
	virtual ReferenceTarget* GetRef(size_t n) = 0;

	/**Allow 'Set' access to the references through this interface
	\param n - The index of the parameter to set.
	If n is invalid, (out of range, or a released reference), we assert
	and return with no changes. 
	\param pTarget - The new reference target */
	virtual void SetRef(size_t n, ReferenceTarget* pTarget) = 0;

	/**Find the first index with the given reference target assigned.
		This function _must_ succeed */
	virtual int GetReferenceIndex(RefInfo* pRefInfo) = 0;

protected:

	/**Tests to see n is the index of a valid reference
	This function tests to see if Get/Set operations can legally
	be called for the given index.  It does not test for reference
	NULL-ness, it only tests to see if this index can be used.
	\param n - the index of the reference to test.
	\return true if we can access this reference index. */
	virtual bool IsValidReferenceIndex(size_t n) = 0;

	/** Directly calling this function is NOT recommended, See Instead RefPtr & RefArray
	This function specifies the given baseID as being an array.
	Registering a BaseID as an array allows a client to create multiple references under the same BaseID.  This is 
	only necessary for a client that wishes to dynamically allocate references during runtime.  
	Failing to declare a BaseID as an array will trigger run time asserts when attempting to register multiple
	references, although the BaseID will then automatically be marked as an array, and allocations will succeed.
	It is not necessary to register a reference under this BaseId already.

	\sa RegisterReference
	
	NOTE: While it is possible for a client to manage their own reference arrays, its highly 
	advised to use the provided container RefArray.
	\param baseID - The Id of the reference group to be marked as a dynamic array.  See RefPtr and RefArray
	\return true on success, false on fail. */
	virtual bool RegisterReferenceArray(size_t baseId)=0;

	/**Directly calling this function is NOT recommended, See Instead RefPtr & RefArray
	Registers a new reference pointer for the given BaseID (and index, if BaseID is an array)
	This function is called to create new references pointers.  It is not recommended to call this
	function directly, instead use the management classes RefPtr & RefArray. 
	For clients wishing to manually manage their references - 
	Also note that it is valid to call this function at any time - HOWEVER,
	it is important that any references dynamically allocated MUST be managed
	during a Save/Load sequence so that on Load we can recreate equivalent reference arrays.
	\param baseId - This is the id of the reference group to create a new reference for.  See RefPtr::BASE_ID.  This
					function should only be called once per unique baseId, unless the BaseID has been registered as an Array
					(see RegisterReferenceArray).
	\param index - If BaseID has been registered as an array, index refers to the index of the reference in this array.
					If baseId is NOT an array, index should be 0.  If BaseID is an array, an index of -1 indicates
					the new reference should be created at the end of the array
	\param ref - An initial value to set our reference to.
	\param isWeak - Specifies the new reference to be a 'weak' reference.  See ReferenceMaker::IsRealDependency
	\param isPersisted - Specifies the reference as temporary (not saved).  See ReferenceMaker::ShouldPersistWeakRef
	\return The RefInfo structure for the newly created reference if successful, else NULL */
	virtual RefInfo* RegisterReference(size_t baseId, int index, NotifyCallback* callback, ReferenceTarget* ref=NULL, bool isWeak = false, bool isPersisted = true)  = 0;

	/** Directly calling this function is NOT recommended, See Instead RefPtr & RefArray
	This is only necessary if the user is implementing dynamic reference management.
	instead a user should use RefArray, which handles all this stuff for you!  If a user
	directly managing their own references, they must release all references acquired
	via RegisterReference when they are no longer required.
	
	Releases a reference created via RegisterReference.
	Releasing a reference removes the reference to the current reference target,
	and deletes the associated RefInfo for this reference.  This may cause NumRefs()
	to return a different number.  A released reference may be re-registered.
	\param pInfo - The RefInfo returned from RegisterReference for this reference
	\param baseId - The Id of the reference group this RefInfo was registered in. 
		\sa RefPtr::BASE_ID and RefArray::BASE_ID */
	virtual RefResult ReleaseReference(RefInfo* pInfo, size_t baseId) = 0;
};