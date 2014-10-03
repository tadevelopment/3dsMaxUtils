
#pragma once

#include <hold.h>

// This file contains general restore objects for managing restore
// for any piece of data.

template<class T> class DataRestoreObj;

// This class is the mechanism for owning classes to receive a 
// callback when it's data is changed because of an undo or redo.
// Note: this class is entirely optional - if no callback is
// required simply hold the data and move on.
template<class T>
class IDataRestoreOwner
{
public:
	IDataRestoreOwner<T>() { }
	virtual ~IDataRestoreOwner<T>() { }

	/// This callback will be called whenever the held
	/// data pointer is changed, either due to an undo or redo
	/// The owning class should implement this function if it
	/// needs to take action  (eg update UI) whenever the data changes.
	/// This function will be called after the value has changed.
	/// \param val The new value just set.
	virtual void OnRestoreDataChanged(T val) = 0;
};

//
// Create an instance of this class for any data you want undone.
// usage:
// int mSomeClassVar;
// theHold.Put(new DataRestoreObj<int>(mSomeClassVar);
//
// !!!Important!!!
// This undo entity should not be used for data stored within an array (or any
// storage where the memory would be reallocated).  If you wish to hold
// For Example:
// <code>
// Tab<int> mSomeClassArray;
// theHold.Put(new DataRestoreObj<int>(mSomeClassArray[0]); // <-- Very Bad, references Tab memory
// mSomeClassArray.Resize(3) // This will reallocate the memory, and the pointer stored by our undo object is no longer valid
// theHold.Cancel(); // Our DataRestoreObj will attempt to write to an invalid pointer address.
// <endcode>
// When the tab is resized that memory address becomes invalid.  It is no longer possible to restore to the original
// memory pointer.
// Note: the following is safe, because the memory pointer stored is always valid, and the data is
// copied back and forth - no heap pointers involved!
// <code>
// Tab<int> mSomeClassArray;
// theHold.Put(new DataRestoreObj<Tab<int>>(mSomeClassArray);
// <endcode>
template<class T>
class DataRestoreObj : public RestoreObj {
private:
	IDataRestoreOwner<T> *mpOwner; /// This should be a pointer to the class that where the data pointed at by mpValue is member
	T *mpValue;		/// The pointer to the data being held
	T mRedo;		/// The value of mpValue after hold is complete, mpValue will be set to this value on redo.
	T mUndo;		/// The value of mpValue when this class is created, mpValue will be set to this value on undo.

	// All functions are private.  There is no need for an external
	// entity to create this class or call its functions.  Call HoldData instead

	DataRestoreObj(T& val, IDataRestoreOwner<T> *pOwner = NULL) 
		: mUndo(val)
		, mRedo(val) // default Redo to Undo, because we can't supply a default value like 0 (may not make sense for every type)
		, mpOwner(pOwner)
		, mpValue(&val)
	{
		DbgAssert(!IsPointerHeld(mpValue));
		SetPointerHeld(mpValue);
	}

	~DataRestoreObj() {};
	
	// Restore *mpValue to its initial value.
	virtual void Restore(int isUndo)	
	{	
		*mpValue = mUndo; 
		if (mpOwner != NULL) 
			mpOwner->OnRestoreDataChanged(mUndo);
	}

	// Restore mpValue to its final value
	virtual void Redo()
	{
		*mpValue = mRedo;
		if (mpOwner != NULL)
			mpOwner->OnRestoreDataChanged(mRedo);
	}

	virtual int Size()		{	return sizeof(this); }
	
	virtual void EndHold()					
	{
		mRedo = *mpValue; 
		EndPointerHold(mpValue);
	}

	// Allow a factory to create this class.
	template<class T>
	friend void HoldData(T& data, IDataRestoreOwner<T>* pOwner);
};

// This class can be used to hold data from within an array, if the
// user does not want to hold the entire array.  Pass the array and
// the index to be held.  Call HoldTabData to use this class.
// The class can handle resis
template<class T>
class TabDataRestoreObj : public RestoreObj {
private:
	IDataRestoreOwner<T> *mpOwner; /// This should be a pointer to the class that where the data pointed at by mpValue is 
	Tab<T>* mpTab;	/// A pointer to the tab that contains the data we want to hold.
	int mDataIndex;	/// The index in the tab of the data we want to hold.
	int mUndoSize;	/// The size of the tab when undo starts
	int mRedoSize;	/// The size of the tab when undo ends
	T mUndo;		/// The value to set on Undo
	T mRedo;		/// The value to set on Redo

	// All functions are private.  Users never need interact with this class directly.

	TabDataRestoreObj(Tab<T>& tab, int index, IDataRestoreOwner<T> *pOwner = NULL) 
		: mpOwner(pOwner)
		, mpTab(&tab)
		, mUndoSize(tab.Count())
		, mRedoSize(tab.Count())
		, mDataIndex(index)
	{
		if (mDataIndex < mUndoSize)
		{
			mRedo = mUndo = tab[mDataIndex];
		}
		DbgAssert(!IsTabPointerHeld(mpTab, mDataIndex));
		SetTabPointerHeld(mpTab, mDataIndex);
	}

	~TabDataRestoreObj() { };

	virtual void Restore(int isUndo)	
	{
		mpTab->SetCount(mUndoSize);
		if (mDataIndex < mUndoSize)
			(*mpTab)[mDataIndex] = mUndo;

		if (mpOwner != NULL) 
			mpOwner->OnRestoreDataChanged(mUndo);
	}

	// Restore mpValue to its final value
	virtual void Redo()
	{
		mpTab->SetCount(mRedoSize);
		if (mDataIndex < mRedoSize)
			(*mpTab)[mDataIndex] = mRedo;
		if (mpOwner != NULL)
			mpOwner->OnRestoreDataChanged(mRedo);
	}

	virtual int Size()		{	return sizeof(this); }

	virtual void EndHold()					
	{
		mRedoSize = mpTab->Count();
		if (mDataIndex < mRedoSize)
			mRedo = (*mpTab)[mDataIndex]; 

		EndTabPointerHold(mpTab, mDataIndex);
	}

	// Allow a factory to create this class, if necessary.
	template<class T>
	friend void HoldTabData(Tab<T>& tab, int index, IDataRestoreOwner<T>* pOwner);
};

// Test to see if the memory pointed at by ptr is
// already held in the undo system somewhere.  If
// it is already being held, then adding a second
// undo pointer is unnecessary (and potentially a bug).
extern bool IsPointerHeld(void* ptr);
extern void SetPointerHeld(void* ptr);
extern void EndPointerHold(void* ptr);

extern bool IsTabPointerHeld(void* ptr, int index);
extern void SetTabPointerHeld(void* ptr, int index);
extern void EndTabPointerHold(void* ptr, int index);

template<class T>
void HoldData(T& data, IDataRestoreOwner<T>* pOwner = NULL)
{
	// This is now the only function we should call.
	// It will automatically register a hold object
	// if necessary.  If the data is already being held
	// it will not double-register the data.
	if (theHold.Holding())
	{
		if (!IsPointerHeld(&data))
		{
			theHold.Put(new DataRestoreObj<T>(data, pOwner));
		}
	}
}

template<class T>
void HoldTabData(Tab<T>& data, int index, IDataRestoreOwner<T>* pOwner = NULL)
{
	// This is now the only function we should call.
	// It will automatically register a hold object
	// if necessary.  If the data is already being held
	// it will not double-register the data.
	if (theHold.Holding())
	{
		if (!IsTabPointerHeld(&data, index))
		{
			theHold.Put(new TabDataRestoreObj<T>(data, index, pOwner));
		}
	}
}