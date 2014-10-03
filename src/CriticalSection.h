#pragma once
#include <winbase.h>

class CriticalSection
{
public:
	CriticalSection()
	{ ::InitializeCriticalSection(&m_rep); }
	~CriticalSection()
	{ ::DeleteCriticalSection(&m_rep); }

	void Enter()
	{ ::EnterCriticalSection(&m_rep); }
	void Leave()
	{ ::LeaveCriticalSection(&m_rep); }

private:
	// copy ops are private to prevent copying
	CriticalSection(const CriticalSection&);
	CriticalSection& operator=(const CriticalSection&);

	CRITICAL_SECTION m_rep;
};

class CSLock
{
public:
	CSLock(CriticalSection& a_section)
		: m_section(a_section) { m_section.Enter(); }
	~CSLock()
	{ m_section.Leave(); }

private:
	// copy ops are private to prevent copying
	CSLock(const CSLock&);
	CSLock& operator=(const CSLock&);

	CriticalSection& m_section;
};