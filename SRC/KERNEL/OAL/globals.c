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
#include <x86ioctl.h>
#include <oal.h>
#include <pc.h>

#define IOCTL_ALADDIN_GET_INFO			CTL_CODE(FILE_DEVICE_HAL,800, METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_ALADDIN_GET_PCCARD_PATH	CTL_CODE( FILE_DEVICE_HAL , 808 , METHOD_BUFFERED , FILE_ANY_ACCESS )

extern BOOL x86IoCtlGetAladdinInfo (
    UINT32 code, VOID *lpInBuf, UINT32 nInBufSize, VOID *lpOutBuf, 
    UINT32 nOutBufSize, UINT32 *lpBytesReturned
);
extern BOOL x86IoCtlGetCardPath (
    UINT32 code, VOID *lpInBuf, UINT32 nInBufSize, VOID *lpOutBuf, 
    UINT32 nOutBufSize, UINT32 *lpBytesReturned
);


const OAL_IOCTL_HANDLER g_oalIoCtlTable [] = {
    { IOCTL_ALADDIN_GET_INFO, 0, x86IoCtlGetAladdinInfo },
    { IOCTL_ALADDIN_GET_PCCARD_PATH, 0, x86IoCtlGetCardPath },
#include <ioctl_tab.h>
};

LPCWSTR g_oalIoCtlPlatformType = L"CEPC";           // changeable by IOCTL
LPCWSTR g_oalIoCtlPlatformOEM  = L"CEPC";           // constant, should've never changed
LPCSTR  g_oalDeviceNameRoot    = "CEPC";            // ASCII, initial value

LPCWSTR g_pszDfltProcessorName = L"i486";

const BOOL g_fSupportHwDbg = TRUE;          // support hardware debugging
const DWORD g_dwBSPMsPerIntr = 1;           // interrupt every ms
const UCHAR g_ucDlftKitlAdaptorType = EDBG_ADAPTER_NE2000;

//
// perform RAM auto-detect if RAM ends at CEPC_EXTRA_RAM_START, for 
// size CEPC_EXTRA_RAM_SIZE.
//
const DWORD g_dwRAMAutoDetectStart = CEPC_EXTRA_RAM_START;
const DWORD g_dwRAMAutoDetectSize  = CEPC_EXTRA_RAM_SIZE;
