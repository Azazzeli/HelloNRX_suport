

#pragma once

//-----------------------------------------------------------------------------
//- 'DEBUG workaround' below prevents the MFC or ATL #include-s from pulling 
//- in "Afx.h" that would force the debug CRT through #pragma-s.
#if defined(_DEBUG) && !defined(NC_FULL_DEBUG)
#define _DEBUG_WAS_DEFINED
#undef _DEBUG
#pragma message ("     Compiling header files in release mode.")
#endif

#pragma pack (push, 8)
#pragma warning(disable: 4786 4996)

// Including SDKDDKVer.h defines the highest available Windows platform.

// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.
#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

// Windows Header Files:
#if !defined(HOST_NO_MFC)
#define _AFXDLL
#include <afxwin.h>
#include <unknwn.h>
#else
#include <windows.h>
#endif
#include "arxHeaders.h"
#ifndef HELLONRX_MODULE
#define HELLONRX_MODULE
#endif
#pragma pack (pop)

//-----------------------------------------------------------------------------
#ifdef _DEBUG_WAS_DEFINED
#define _DEBUG
#undef _DEBUG_WAS_DEFINED
#endif

