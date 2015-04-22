#pragma once

class ViewExp;
class Interface;

//
// This file is to ease supporting multiple versions of Max.
// Functions which have been deprecated, or definitions which
// have been changed can be channeled through this file to get
// a consistent point-of-entry into Max.
//

ViewExp* GetViewport(Interface* pCore, HWND hWnd);

ViewExp* GetActiveViewport(Interface* pCore);

void ReleaseViewport(Interface* pCore, ViewExp* pView);