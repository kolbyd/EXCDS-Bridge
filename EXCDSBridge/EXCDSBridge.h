// EXCDSBridge.h : main header file for the EXCDSBridge DLL
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// CEXCDSBridgeApp
// See EXCDSBridge.cpp for the implementation of this class
//

class CEXCDSBridgeApp : public CWinApp
{
public:
	CEXCDSBridgeApp();

// Overrides
public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
