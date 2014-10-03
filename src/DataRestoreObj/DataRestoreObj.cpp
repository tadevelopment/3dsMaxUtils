#include "StdAfx.h"

#include "DataRestoreObj.h"
#include <list>
#include <map>

#ifdef _DEBUG
// The following numbers are here to give
// us an idea of what size we can expect PointerList
// to grow to in normal operation.
static size_t sMaxRestoreClasses = 0;		// This is the max concurrent pointers being held
static int sTotalRestoreClasses = 0;	// This is the cumulative number of pointers held
static int sNumBeginEndPairs = 0;		// This is the number of times an undo action has been held.
// The average number of DataRestoreObj's held per undo is sTotal/sNumPairs;
#endif

class PointerList : public std::list<void*>
{
private:
	PointerList() {}; // No
	PointerList(PointerList& ); // No Copy
public:

	static PointerList& GetPointerList()
	{
		static PointerList ptrList;
		return ptrList;
	}
};

// Test to see if this data pointer is already held in the undo system somewhere.
bool IsPointerHeld(void* ptr)
{
	PointerList &theList = PointerList::GetPointerList();
	for (PointerList::iterator itr = theList.begin(); itr != theList.end(); itr++)
	{
		if (*itr == ptr)
			return true;
	}
	return false;
}

void SetPointerHeld(void* ptr)
{
	if (IsPointerHeld(ptr) || NULL == ptr)
	{
		// We don't double-hold pointers
		return;
	}
	PointerList& theList = PointerList::GetPointerList();

#ifdef _DEBUG
	if (theList.begin() == theList.end())
	{
		sNumBeginEndPairs++;
	}
	sTotalRestoreClasses++;
#endif

	theList.push_back(ptr);
}

void EndPointerHold(void* ptr)
{
	DbgAssert(IsPointerHeld(ptr) && _T("ERROR: Ending hold on non-held pointer"));
	PointerList& theList = PointerList::GetPointerList();

#ifdef _DEBUG
	if (theList.size() > sMaxRestoreClasses)
		sMaxRestoreClasses = theList.size();
#endif

	theList.remove(ptr);
}

//////////////////////////////////////////////////////////////////////////

class TabMap : public std::map<void*, std::list<int>>
{
private:
	TabMap() {}; // No
	TabMap(TabMap& ); // No Copy

public:

	static TabMap& GetTabMap()
	{
		static TabMap map;
		return map;
	}
};


// Test to see if this data pointer is already held in the undo system somewhere.
bool IsTabPointerHeld(void* ptr, int index)
{
	TabMap &theMap = TabMap::GetTabMap();
	// First try and find the tab, if it's held
	TabMap::iterator itr = theMap.find(ptr);
	if (itr != theMap.end())
	{
		// If the tab is held, was this particular index held?
		std::list<int>& heldIndices = itr->second;
		for (std::list<int>::iterator itr = heldIndices.begin(); itr != heldIndices.end(); itr++)
		{
			if (*itr == index)
				return true;
		}
	}
	
	return false;
}

void SetTabPointerHeld(void* ptr, int index)
{
	if (NULL == ptr)
		return;

	TabMap& theMap = TabMap::GetTabMap();
#ifdef _DEBUG
	if (theMap.empty())
	{
		sNumBeginEndPairs++;
	}
	sTotalRestoreClasses++;
#endif

	TabMap::iterator itr = theMap.find(ptr);
	if (itr != theMap.end())
	{
		// The tab is already held - double check
		// that the index has not been held.
		DbgAssert(!IsTabPointerHeld(ptr, index));
		itr->second.push_back(index);
	}
	else
	{
		// Create a new item to hold
		std::list<int> newList;
		newList.push_back(index);
		theMap.insert(std::pair<void*, std::list<int>>(ptr, newList));
	}
}

void EndTabPointerHold(void* ptr, int index)
{
	DbgAssert(IsTabPointerHeld(ptr, index) && _T("ERROR: Ending hold on non-held pointer"));
	TabMap& theMap = TabMap::GetTabMap();

#ifdef _DEBUG
	if (theMap.size() > sMaxRestoreClasses)
		sMaxRestoreClasses = theMap.size();
#endif

	TabMap::iterator itr = theMap.find(ptr);
	if (itr->second.size() == 1) // This is the last element
		theMap.erase(itr);
	else
	{
		itr->second.remove(index);
	}
}