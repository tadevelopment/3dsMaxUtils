#include "maxapi.h"
#include "MaxVersionResolve.h"

ViewExp* GetViewport( Interface* pCore, HWND hWnd )
{
#if MAX_VERSION_MAJOR < 15
	return pCore->GetViewport(hWnd);
#else
	return &pCore->GetViewExp(hWnd);
#endif
}

ViewExp* GetActiveViewport( Interface* pCore )
{
#if MAX_VERSION_MAJOR < 15
	return pCore->GetActiveViewport();
#else
	return &pCore->GetActiveViewExp();
#endif
}

void ReleaseViewport( Interface* pCore, ViewExp* pView )
{
#if MAX_VERSION_MAJOR < 15
	pCore->ReleaseViewport(pView);
#else
	pView; pCore; // No compiler warning
#endif
}

