//
// Copyright 2009 Autodesk, Inc.  All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license
// agreement provided at the time of installation or download, or which
// otherwise accompanies this software in either electronic or hard copy form.  
//

#pragma once

#include "IReferenceManager.h"
#include "../MaxVersionSelector.h"
#include <Containers/Array.h>
#include <vector>
//=========================================================
/// This class provides the implementation of the IReferenceManager and 
/// ReferenceMaker interface.  All functions are implemented by the system
///
/// This class is used for managing arbitrary numbers of reference targets. 
/// All pointers used by classes for non-trivial lengths of time should be stored as references
///
/// Developers should not normally call functions on this class
/// directly.  The RefPtr templates provide a safe and immediate way to 
/// use ReferenceManager transparently.  RefPtr also allows developers to
/// treat references as simple pointers safely.
///
/// The NotifyRefChanged function is the only function in this class the client
/// needs to be aware of.  This function may be implemented to allow custom handling
/// of any Change notification messages sent by the reference targets.
///
/// The template parameter Base_T is used as the Base class. 
//=========================================================

template<typename Base_T, int USE_BASE_REF=0>
class ReferenceManager 
    : public Base_T, public virtual IReferenceManager
{
    //========================================================================
#pragma region // Class variables

private:

    // This manages an array of reference targets. The template class "RefMgr" is not 
    // used because it may be deprecated in later versions of the 3ds Max 2010 SDK.
    // The class IRefTargContainer is not used because it does not support node targets,
	MaxSDK::Array<RefInfo*> m_refs;

	// Stores the number of references in each array
	std::vector<size_t> m_arraySizes;

	// Stores the index of the last static reference
	// Every dynamic reference must be at a higher index than this.
	size_t m_baseDynIdx;

	// disable copy
	ReferenceManager& operator=(ReferenceManager& rhs);

public:

	/// This constant is provided to enforce base classes to
	/// to provide an accurate total count of their references.
	/// kBaseIndex should then become the lowest index for the
	/// references of the deriving class, to allow each class to
	/// manage references independently.
	/// \code
	/// class BaseClass : ReferenceManager {
	///		enum MyRefInfo {
	///			kBaseIndex = ReferenceManager::kNumRefs,
	///			kRef1,
	///			kRef2,
	///			kNumRefs,
	///		}
	/// };
	///
	/// class DerClass : BaseClass {
	///		enum MyRefs {
	///			kBaseIndex = BaseClass::NumRefs,
	///			kRef1,
	///			kRef2,
	///			kNumRefs,
	///		}
	/// };
	// TODO: Fix/Add this back if/when we are integrated into the SDK
	enum MyRefInfo {
		kBaseIndex = 0, // USE_BASE_REF ? Base_T::kNumRefs : 0
		kNumRefs = kBaseIndex
	};

#pragma endregion // Class variables

    //========================================================================
#pragma region // Constructor/Destructor

protected:

    //========================================================================
    // Constructor  destructor

    ReferenceManager()
        : Base_T()
		, m_baseDynIdx(INT_MAX) // Until we register an array, all refs are static
    {
		// Compiler safety - Ensure that Base_T class to derive from ReferenceTarget somehow
		ReferenceMaker::GetReference(0);
    }

    ~ReferenceManager() 
    {
		// Double check - these are all released, right?
		for (size_t i = 0; i < m_arraySizes.size(); i++)
		{
			DbgAssert(m_arraySizes[i] == 0 && "LEAK - Dynamic Reference not released!");
		}
		for (size_t i = 0; i < m_refs.length(); i++)
		{
			// If the above assert triggers, this one definately will as well.
			DbgAssert(m_refs[i] == NULL && "LEAK - A Reference was not released!");
		}

        // Informs 3ds Max that it is safe to delete the all references from and to this object
        DeleteAllRefs();
    }

	// This function allows us to refer to ourselves
	// in the deriving classes constructor
	// 
	// MyClass::MyClass : somePtr(this) // Will not compile
	// MyClass::MyClass : somePtr(GetRefMgr()) // is legal
	IReferenceManager& GetRefMgr() {
		return *this;
	}

#pragma endregion // Constructor/Destructor

    //============================================================
#pragma region // ReferenceMaker/Target overloads

public:

	/// Return the total references managed by this class
	virtual int NumRefs() 
    {
		// Allow for parent classes with references
        return kBaseIndex + (int)m_refs.length();
    }

	/// Returns a pointer to the i'th reference
    virtual RefTargetHandle GetReference(int i) 
    {
#if kBaseIndex != 0 // Compiling this with kBaseIndex == 0 produces a warning (conditional expression is constant)
		// Allow for parent classes with references
		if (kBaseIndex >= 0 && i < kBaseIndex)
			return Base_T::GetReference(i);
#endif
		// Else, its one that we own.
        //DbgAssert(IsValidReferenceIndex(i));
        //if (!IsValidReferenceIndex(i))
        //    return NULL;

		RefInfo* pInfo = GetInfo(i);
		return (pInfo != NULL) ? pInfo->m_target : NULL;
    }

	/// Returns true if the specified target is a real dependency.
	virtual BOOL IsRealDependency(ReferenceTarget* rtarg)
	{
		int n = GetReferenceIndex(rtarg);
		if (n < kBaseIndex)
			return Base_T::IsRealDependency(rtarg);

		RefInfo* pInfo = GetInfo(n);
		return (pInfo != NULL) ? !pInfo->TestFlag(RefInfo::kIsWeak) : FALSE;
	}

	/// Should the reference to the specified target be saved?
	virtual BOOL ShouldPersistWeakRef(ReferenceTarget* rtarg)
	{
		int n = GetReferenceIndex(rtarg);
		if (n < kBaseIndex)
			return Base_T::IsRealDependency(rtarg);

		RefInfo* pInfo = GetInfo(n);
		return (pInfo != NULL) ? pInfo->TestFlag(RefInfo::kIsPersisted) : TRUE;
	}

protected:

    // This default implementation of BaseClone will clone all objects that have strong references 
    // to them. If you do not desire this behavior, then override this function with your own 
    // implementation

	// NOTE  -this method is not part of reference Maker!
    virtual void BaseClone(ReferenceTarget* from, ReferenceTarget* to, RemapDir& remap) 
    {
        if (!from || !to || from == to)
            return;

	    for (int i=0; i < from->NumRefs(); ++i)
        {
            ReferenceTarget* fromTarget = from->GetReference(i);
            
            // Do not clone weak references, just copy them
            if (from->IsRealDependency(fromTarget))
                to->ReplaceReference(i, remap.CloneRef(fromTarget));
            else
                to->ReplaceReference(i, fromTarget);
        }

        Base_T::BaseClone(from, to, remap);
    }

	// A default implementation simply manages reference deletion.  Override this function
	// to handle any specific reference changes.
    virtual RefResult NOTIFY_REF_CHANGED_FN_DECL
   	{
		UNUSED_PARAM(changeInt);
#if MAX_VERSION_MAJOR > 16
		UNUSED_PARAM(propagate);
#endif
		int n = GetReferenceIndex(hTarget);
		if (n >= kBaseIndex)
		{
			RefInfo* pInfo = GetInfo(n);
			if (pInfo != NULL && pInfo->m_callback != NULL)
				(*pInfo->m_callback)(message, partID);
		}

		switch (message) 
        {
        case REFMSG_TARGET_DELETED:
            
            if (hTarget != NULL)
            {
                int n = GetReferenceIndex(hTarget);
                DbgAssert(n >= 0 && "Internal error, reference could not be found");

#if kBaseIndex > 0 // Only compile this in if we have base references - otherwise we get compiler errors!
				// Its possible that our parent class 
				// does custom handling of this.  Just in
				// case, we sent this message to them
				if (n < kBaseIndex)
					Base_T::NotifyRefChanged(changeInt, hTarget, partID, message );
#endif
				// NULL the pointer.
				SetReference(n, NULL);
				return REF_SUCCEED;
            }
        }

        return REF_SUCCEED;
    }

private:

	// Internal only.  Do not call this function.
	virtual void SetReference(int i, RefTargetHandle rtarg) 
	{ 
		DbgAssert(IsValidReferenceIndex(i));
		if(i < kBaseIndex)
			Base_T::SetReference(i, rtarg);
		else
		{
			RefInfo* pInfo = GetInfo(i);
			if (pInfo != NULL) pInfo->m_target = rtarg;
		}
	}

#pragma endregion // ReferenceMaker functions

    //========================================================================
#pragma region // IReferenceManager derived methods

private:

	// Get/Set should be only used the template pointer classes
	RefTargetHandle GetRef(size_t n) { return GetInfo(n)->m_target; }
	void SetRef(size_t n, ReferenceTarget* pTarget) { 
		ReplaceReference((int)n, pTarget); 
		// Our only hack - sometimes we will be notified of a target
		// being deleted. If this happens, the original code was
		// if (msg == REFMSG_TARGET_DELETED) ptr = NULL;
		// Then we will trigger a ReplaceReference rather
		// than setting the ptr.  In this case, calling ReplaceReference
		// will fail because the ptr has already been removed from the RefList.
		// We need to explicitly set the value of our pointer array
		if (pTarget == NULL) 
			SetReference((int)n, pTarget); // Just because the manager does it, you still shouldnt call SetReference
	}

public:

    // Returns true if n is a valid reference
    bool IsValidReferenceIndex(size_t n)
    {
		if (n >= 0)
		{
			if (n < kBaseIndex)
				return TRUE;
			return GetInfo(n) != NULL;
		}
		return false;
    }

    int GetReferenceIndex(ReferenceTarget* ref) 
    {
        for (int i=0; i < NumRefs(); ++i)
            if (GetReference(i) == ref)
                return i;
        return -1;
    }

	int GetReferenceIndex(RefInfo* ref) 
	{
		for (int i=0; i < NumRefs(); ++i)
			if (GetInfo(i) == ref)
				return i;
		return -1;
	}

	int GetBaseReferenceIndex() 
	{
		return kBaseIndex;
	}

	// Casts a reference to a specific type.
	// This is a helper function for those who 
	// do not want to use the RefPtr template classes.
	template<typename T>
	T GetReferenceAs(size_t n) 
	{
		return dynamic_cast<T>(GetReference(n));
	}

	RefInfo* GetInfo(size_t refId) { 
		if (refId >= kBaseIndex && refId < m_refs.length() + kBaseIndex) 
			return m_refs[refId - kBaseIndex]; 
		return NULL;
	}

#pragma endregion // IReferenceManager derived methods

	//========================================================================
#pragma region // Reference Registration (Insert/Release) functions

protected:

    // The n parameter is not strictly necessary, because it should always be equal to the number 
    // of references. This is the contract of using this function. By requiring it though, we 
    // help plug-in users be explicit about what the indexes of their references are.
    // Important Note: do not call RegisterReference after construction. All fully constructed 
    // instances of a plug-in must have the same number of references if they want to 
    // use ReferenceManager. Returning REF_FAIL most likely indicates a circular reference. 
    RefResult RegisterReference(size_t n, NotifyCallback* callback, ReferenceTarget* ref, bool isWeak = false, bool isPersisted = true) 
    {
		DbgAssert(!IsValidReferenceIndex(int(n)) && "Cannot register reference to a live index");
		
		// Add a new RefInfo structure to the array.
		InsertReference(int(n), callback, isWeak, isPersisted);
		
		// We have to call ReplaceReference to set the reference so that 3ds Max can 
        // track the reference correctly. This will also check that result is not a circular reference
        RefResult result = ReplaceReference(int(n), ref);
        
        return result;           
    }  

	bool RegisterReferenceArray(size_t arrayIdx)
	{
		// Ensure that arrayIdx is within range of m_refs
		//int arrayRefIdx = GetReferenceIndexForArray(arrayIdx);
		if (arrayIdx >= m_refs.length())
		{
			if (arrayIdx > m_baseDynIdx)
			{
				// If we have arrays below us, we no longer
				// can assume that arrayIdx == our refIdx;
				// Calculate what it should be...
				size_t nArrays = m_arraySizes.size();
				if ((size_t)nArrays < arrayIdx)
				{
					// How refs do we have currently?
					size_t nRefs = m_refs.length();
					size_t nNewRefs = 1 + arrayIdx - (m_baseDynIdx + m_arraySizes.size());
					// If the above tries to go negative, our unsigned size_t
					// becomes a Very Large Number
					DbgAssert(nNewRefs < INT_MAX);
					if (nNewRefs < INT_MAX)
						m_refs.setLengthUsed(nRefs + nNewRefs, NULL);
				}
			}
			else if (m_arraySizes.size() == 0)
			{
				size_t nNewLength = arrayIdx + 1;
				m_refs.setLengthUsed(nNewLength, NULL);
			}
		}

		// Ensure that we record the lowest-indexed
		// array created (this is so that refs that are
		// lower indexed than arrays can skip all the
		// array bookkeeping stuff).  This is all essentially mem optimization
		if (arrayIdx < m_baseDynIdx)
		{
			// Now, convert all existing RefPtr's higher than
			// our new base index to 1-sized arrays
			size_t maxIdx = min(m_baseDynIdx, m_refs.length());
			size_t numToConvert = maxIdx - arrayIdx;
			DbgAssert(numToConvert < INT_MAX);

			// Now, convert numToConvert previously static references to dynamic ones
			size_t *dNewIndices = new size_t[numToConvert];
			// init following indices to 1 (thats the size of the array they represent)
			for (size_t i = 0; i < numToConvert; i++)
				dNewIndices[i] = 1;
			m_arraySizes.insert(m_arraySizes.begin(), dNewIndices, dNewIndices + numToConvert);
			delete dNewIndices;
			// Our array index is the new lowest dynamic index
			m_baseDynIdx = int(arrayIdx);
		}
		else
		{
			// We are either converting an unused index to an array, or 
			// we have allocated a new array.  If its an unused index. we don't
			// need to worry about it.  Otherwise, we just need to extend our
			// array counts to include this new array.
			size_t numArrays = 1 + arrayIdx - m_baseDynIdx;
			if ((int)numArrays > m_arraySizes.size())
			{
				size_t numToConvert = numArrays - m_arraySizes.size();
				size_t *dNewIndices = new size_t[numToConvert];
				// init new indices to 1 (thats the size of the array they represent)
				for (size_t i = 0; i < numToConvert; i++)
					dNewIndices[i] = 1;
				m_arraySizes.insert(m_arraySizes.end(), dNewIndices, dNewIndices + numToConvert);
				delete [] dNewIndices;
			}
		}

		arrayIdx -= m_baseDynIdx;
		int nRefIdxForArray = GetReferenceIndexForArray(arrayIdx, 0);

		// The following code is to ensure that
		// the we can define RefPtrs as:
		// RefPtr<4, PTarg>
		// and without any other reference pointers
		// defined, it will still create the following
		// ref array [NULL, NULL, NULL, PTarg*].
		// This is necessary to allow arbitrary
		// initialization orders
#ifdef _DEBUG
		RefInfo* pInfo = GetInfo(nRefIdxForArray);
		DbgAssert(pInfo == NULL && "ERRROR: Assigning Array to a non-empty index");
		DbgAssert(m_arraySizes[arrayIdx] == 1);
#endif
		// When the above(NULL) RefInfo pointer was originally
		// allocated, it was under the assumption it was
		// for a static index (ie - it represented a RefPtr
		// that just didn't exist yet).  We are essentially
		// converting it to represent an array, which
		// has a size of 0.  To do this, we remove the NULL
		// RefPtr currently at this index - NOTE: This will
		// decrease the index of all the higher RefPtrs by 1
		m_refs.removeAt(nRefIdxForArray);
		m_arraySizes[arrayIdx] = 0;
		return true;
	}

	// Dynamic array sizes.  Only accessible from ReferenceManager
	RefResult ReleaseReference(RefInfo* pInfo)
	{
		DbgAssert(pInfo != NULL);

		// Find our target
		int n = GetReferenceIndex(pInfo);
		
		// We own this reference still, right?
		DbgAssert(n >= 0);
		if (n < 0)
			return REF_FAIL;
		DbgAssert(pInfo == GetInfo(n));
		
		// Very important - this reference has now gone away!
		if (pInfo->m_target != NULL)
		{
			DeleteReference(n);
			DbgAssert(pInfo->m_target == NULL);
		}

		// resize the actual array
		// Only resize the array if the ref info is dynamic
		bool bResizeArray = ((size_t) n >= m_baseDynIdx);
		if (bResizeArray)
		{
#ifdef _DEBUG
			bool res = 
#endif
				m_refs.remove(pInfo);
			DbgAssert(res == true);
		}
		else
			m_refs[m_refs.find(pInfo)] = NULL;
		
		// References cleaned up.  Delete the info
		delete pInfo;
		// Success!
		return REF_SUCCEED;
	}

	RefInfo* InsertReference(int n, NotifyCallback* callback, bool isWeak = false, bool isPersisted = true) 
    {
		// -ve number means just the last one.
		if (n < 0)
			n = NumRefs();

		// Add a new RefInfo structure to the array.
		while (n >= NumRefs())
			m_refs.append(NULL);

		// Ensure we insert at the appropriate index!
		// Watch out - as m_refs.length() doesnt necessarily == NumRefs
		if (GetInfo(n) != NULL)
			m_refs.insertAt(n-kBaseIndex, NULL);

		// Validate this all
		DbgAssert(GetInfo(n) == NULL);

		// Create the reference object
		RefInfo* newInfo = new RefInfo();
		newInfo->SetIsWeak(isWeak);
		newInfo->SetIsPersisted(isPersisted);
		newInfo->m_callback = callback;
		m_refs[n-kBaseIndex] = newInfo;

		// This is almost like unit testing
		DbgAssert(GetInfo(n) == newInfo);
        
        DbgAssert((isWeak || isPersisted == TRUE) && "Strong references are always persisted automatically");

		return newInfo;           
    }   

	RefInfo* RegisterReference(size_t arrayIdx, int index, NotifyCallback* callback, ReferenceTarget* ref=NULL, bool isWeak = false, bool isPersisted = true)
	{
		// If we are a static index ref, it is because
		// there are no dynamic refs lower than us
		if (arrayIdx < m_baseDynIdx)
		{
			// This is a static reference (not an array).
			DbgAssert(index == 0 && "ERROR: Trying to assign an array reference to a static index");
			// Register a bog-standard static-idx ref
			DbgAssert(GetInfo(arrayIdx) == NULL && "ERROR: Trying to assign to existing slot");
			RefResult res = RegisterReference(arrayIdx, callback, ref, isWeak, isPersisted);
			res;
			DbgAssert(res == REF_SUCCEED);
			return GetInfo(arrayIdx);
		}

		// Translate from the index of our array as a reference
		// to the index in our arraySizes array
		arrayIdx -= m_baseDynIdx;

		// We automatically account for any new arrays without requiring registration of the array itself
		if (arrayIdx >= (size_t)m_arraySizes.size())
		{
			size_t oldCount = m_arraySizes.size();
			m_arraySizes.resize(arrayIdx + 1);
			for (size_t i = oldCount; i <= arrayIdx; i++)
				m_arraySizes[i] = 1;
		}
		else
		{
			// Our array has grown by size 1
			IncrementArrayCount(arrayIdx);
		}

		// index == the index in this array. -1 means at the end of the
		// array, which is the array size
		if (index < 0)
			index = int(m_arraySizes[arrayIdx] - 1); // Our size has been incremented already

		// Find the reference index for this array and offset
		size_t n = GetReferenceIndexForArray(arrayIdx, index);
		// Create the reference at this index
		RefInfo* pInfo = InsertReference(int(n), callback, isWeak, isPersisted);
		DbgAssert(pInfo != NULL && pInfo == GetInfo(n) && "ERROR: Inserted reference doesn't match specified index");
		if (pInfo == NULL) 
			return pInfo;

		// Assign ref
		if (ref != NULL)
			ReplaceReference(int(n), ref);
		DbgAssert(GetReference(int(n)) == ref);

		return pInfo;
	}

	// Dynamic array sizes.  Only accessible from RefPtr
	RefResult ReleaseReference(RefInfo* pInfo, size_t arrayIdx)
	{
#ifdef _DEBUG
		// Find our target
		size_t refIdx = GetReferenceIndex(pInfo);
		// We cannot release a static ref via this method
		//DbgAssert(refIdx >= m_baseDynIdx);
		// And check _everything_
		DbgAssert(pInfo == GetInfo(refIdx));
#endif

		RefResult res = ReferenceManager::ReleaseReference(pInfo);
		DbgAssert(res == REF_SUCCEED);
		if (res != REF_SUCCEED)
			return res;

		// We may not have an array at this index.
		if (arrayIdx < m_baseDynIdx)
			return REF_SUCCEED;

		// remove static indices (ones we don't care about) from arrayIdx
		arrayIdx -= m_baseDynIdx;

		// We need to decrement the array this belonged too.
		// Just for debugging, Assert that this refIdx belongs to the right array
#ifdef _DEBUG
		size_t total = m_baseDynIdx;
		for (size_t testArrayIdx = 0; testArrayIdx < m_arraySizes.size(); testArrayIdx++)
		{
			total += m_arraySizes[testArrayIdx];
			// If the current array brings the total to more than RefIdx
			// this array contained that index and needs to be decremented
			if (total > refIdx)
			{
				DbgAssert(testArrayIdx == (size_t)arrayIdx && "ERROR: Didn't find released RefInfo in the right array");
				break;
			}
			if (testArrayIdx > (size_t)arrayIdx)
			{
				DbgAssert("ERROR: Didn't find released RefInfo in the right array");
				break;
			}
		}
#endif
		DecrementArrayCount(arrayIdx);
				
		return REF_SUCCEED;
	}

	// Set the callback for the specified reference
	// This allows derived classes to override the parents
	// callback if necessary.  This function will take
	// ownership of the passed callback and will release it
	// when no longer necessary.  The caller should not
	// maintain a pointer to the callback
	bool SetNotifyCallback(int i, NotifyCallback* pCallback)
	{
		RefInfo* pInfo = GetInfo(i);
		if (pInfo == nullptr)
		{
			delete pCallback;
			return false;
		}
		// If any callback existed prior, delete it.
		delete pInfo->m_callback;
		pInfo->m_callback = pCallback;
		return true;
	}

	// Set the callback for the specified reference
	// This allows derived classes to override the parents
	// callback if necessary.  This function will take
	// ownership of the passed callback and will release it
	// when no longer necessary.  The caller should not
	// maintain a pointer to the callback
	bool SetNotifyCallback(ReferenceTarget* target, NotifyCallback* pCallback)
	{
		return SetNotifyCallback(FindRef(target), pCallback);
	}

	/// Private, local functions

	int GetReferenceIndexForArray(size_t arrayIdx, size_t offset)
	{
		DbgAssert(arrayIdx < m_arraySizes.size());
		if (arrayIdx >= m_arraySizes.size())
			return -1;

		// Count the total up till whereever we are
		size_t total = m_baseDynIdx;
		for (size_t i = 0; i < arrayIdx; i++)
		{
			total += m_arraySizes[i];
		}

		// Is our offset in range?
		//if (offset < 0)
		//	offset = m_arraySizes[arrayIdx];
		DbgAssert(m_arraySizes[arrayIdx] >= offset);

		// The reference index for this array (with offset);
		int refIdx = int(total + offset);
		return refIdx;
	}

	void IncrementArrayCount(size_t arrayIdx)
	{
		DbgAssert(arrayIdx >= 0 && arrayIdx < m_arraySizes.size());
		if (arrayIdx >= 0 && arrayIdx < m_arraySizes.size())
			m_arraySizes[arrayIdx]++;

		// Debugging
		ValidateArrays();
	}

	void DecrementArrayCount(size_t arrayIdx)
	{
		DbgAssert(arrayIdx >= 0 && arrayIdx < m_arraySizes.size());
		if (arrayIdx >= 0 && arrayIdx < m_arraySizes.size())
			m_arraySizes[arrayIdx]--;

		// Debugging
		ValidateArrays();
	}

	void ValidateArrays() {
#ifdef DEBUG
		// Required if we have arrays
		if (m_baseDynIdx >= NumRefs())
			return;

		// Sanity checking only.
		int total = m_baseDynIdx;
		for (int i = idx; i < m_arraySizes.size(); i++)
			total += m_arraySizes.size();
		DbgAssert(total == NumRefs());
#endif
	}

#pragma endregion // RefInfo management functions

};