//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// -----------------------------------------------------------------------------
//
//      THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//      ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//      THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//      PARTICULAR PURPOSE.
//  
// -----------------------------------------------------------------------------
#include <windows.h>
#include <oal.h>
#include <oal_nkxp.h>

//
// handle IOCTL_HAL_TRANSLATE_IRQ for BC support. This IOCTL is being deprecated and
// should not be used.
//
BOOL x86IoCtlHalTranslateIrq (
    UINT32 code, VOID *lpInBuf, UINT32 nInBufSize, VOID *lpOutBuf, 
    UINT32 nOutBufSize, UINT32 *lpBytesReturned
) {

    // Check input parameters
    if (   lpInBuf
        && lpOutBuf
        && (nInBufSize  >= sizeof(UINT32))
        && (nOutBufSize >= sizeof(UINT32))) {

        // Call function itself (in backward compatible mode)
        *(UINT32*)lpOutBuf = OALIntrRequestSysIntr(
            1, (UINT32*)lpInBuf, OAL_INTR_TRANSLATE
        );
        if (lpBytesReturned) {
            *lpBytesReturned = sizeof(UINT32);
        }
        return TRUE;
    }

    NKSetLastError (ERROR_INVALID_PARAMETER);
    return FALSE;

}

BOOL x86IoCtlGetCardPath (
    UINT32 code, VOID *lpInBuf, UINT32 nInBufSize, VOID *lpOutBuf, 
    UINT32 nOutBufSize, UINT32 *lpBytesReturned
) {
	DWORD len;
	LPCTSTR p, pszPath;
	LPCTSTR pszInternalPath = _T("\\RubyFlash\\Internal Card");
	LPCTSTR pszExternalPath = _T("\\RubyFlash\\External Card");
    // Check input parameters
    if (   lpInBuf
        && lpOutBuf
        && (nInBufSize  >= sizeof(UINT32)))
	{
		pszPath = *(BOOL*)lpInBuf ? pszInternalPath : pszExternalPath;
		p = pszPath;
		while (*p++) ;
		len = (p-pszPath)*sizeof(TCHAR);

		if (nOutBufSize >= len)
		{
			memcpy(lpOutBuf, pszPath, len);
			if (lpBytesReturned)
				*lpBytesReturned = len;
			return TRUE;
		} else {
			NKSetLastError (ERROR_INSUFFICIENT_BUFFER);
			return FALSE;
		}
	}

    NKSetLastError (ERROR_INVALID_PARAMETER);
    return FALSE;
}

typedef struct sysOptionsBITS {
	unsigned sysOption0		: 1;
	unsigned sysOption1		: 1;
	unsigned sysOption2		: 1;
	unsigned sysOption3		: 1;
	unsigned sysOption4		: 1;
	unsigned sysOption5		: 1;
	unsigned sysOption6		: 1;
	unsigned sysOption7		: 1;
	unsigned sysOption8		: 1;
	unsigned sysOption9		: 1;
	unsigned sysOption10	: 1;
	unsigned sysOption11	: 1;
	unsigned sysOption12	: 1;
	unsigned sysOption13	: 1;
	unsigned sysOption14	: 1;
	unsigned sysOption15	: 1;
	unsigned sysOption16	: 1;
	unsigned sysOption17	: 1;
	unsigned sysOption18	: 1;
	unsigned sysOption19	: 1;
	unsigned sysOption20	: 1;
	unsigned sysOption21	: 1;
	unsigned sysOption22	: 1;
	unsigned sysOption23	: 1;
	unsigned sysOption24	: 1;
	unsigned sysOption25	: 1;
	unsigned sysOption26	: 1;
	unsigned sysOption27	: 1;
	unsigned sysOption28	: 1;
	unsigned sysOption29	: 1;
	unsigned sysOption30	: 1;
	unsigned sysOption31	: 1;
} RUBY_OPTIONS;

typedef struct _RUBY_LOADER_VARS	// Boot loader environment variables.
{
    DWORD nSig;
    USHORT nMajVer;
    USHORT nMinVer;
    USHORT nKernelMajVer;
    USHORT nKernelMinVer;
    DWORD nIPAddr;
    DWORD nSubnetMask;
    DWORD nUseDHCP;
    DWORD nSPP;
	DWORD nAutoCount;
	DWORD dwLoaderCRC;
	DWORD dwKernelCRC;
    DWORD nAutoLaunch; //1: Loader AutoLaunch WinCE OS. 0: download image
	DWORD nTransport;
    DWORD nLauguage;			//Language
	char  szSerialNumber[32];	//Device Serial Number
	TCHAR szLoaderBuildDate[16];	//Loader build date
	TCHAR szLoaderBuildTime[16];	//Loader build time
	TCHAR szKernelBuildDate[16];
	TCHAR szKernelBuildTime[16];
	TCHAR szDeviceName[16];			//Ruby 100/200/300
	DWORD dwPageCounter;			//Total pages printed by Ruby printer
//	DWORD dwOptions;				//system Options
	RUBY_OPTIONS RubyOptions;				//system Options
	DWORD nDebug;					//Debug mode
	DWORD dwReserved[15];	
	DWORD dwCRC;					//CRC of this structure
} RUBY_LOADER_VARS, *PRUBY_LOADER_VARS;

BOOL x86IoCtlGetAladdinInfo (
    UINT32 code, VOID *lpInBuf, UINT32 nInBufSize, VOID *lpOutBuf, 
    UINT32 nOutBufSize, UINT32 *lpBytesReturned
) {
	PRUBY_LOADER_VARS pRubyVars;

    // Check input parameters
    if (lpOutBuf
        && (nOutBufSize >= sizeof(RUBY_LOADER_VARS))) {
		memset(lpOutBuf, 0, sizeof(RUBY_LOADER_VARS));
		pRubyVars = (PRUBY_LOADER_VARS)lpOutBuf;
		*(DWORD*)&pRubyVars->RubyOptions = 0xFFFFFFFF;

        if (lpBytesReturned) {
            *lpBytesReturned = sizeof(RUBY_LOADER_VARS);
        }
        return TRUE;
    }

    NKSetLastError (ERROR_INVALID_PARAMETER);
    return FALSE;

}
