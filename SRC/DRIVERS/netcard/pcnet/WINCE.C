/*++

Copyright (c) 1998 ADVANCED MICRO DEVICES, INC. All Rights Reserved.
This software is unpblished and contains the trade secrets and
confidential proprietary information of AMD. Unless otherwise provided
in the Software Agreement associated herewith, it is licensed in confidence
"AS IS" and is not to be reproduced in whole or part by any means except
for backup. Use, duplication, or disclosure by the Government is subject
to the restrictions in paragraph (b) (3) (B) of the Rights in Technical
Data and Computer Software clause in DFAR 52.227-7013 (a) (Oct 1988).
Software owned by Advanced Micro Devices, Inc., 901 Thompson Place,
Sunnyvale, CA 94088.

Module Name:  

    wince.c

Abstract:  

Windows CE specific functions for the PCnet NDIS 4.0  miniport driver.

Functions:
    DllEntry
    AddKeyValues
    Install_Driver

Notes: 


--*/

#include <windows.h>
#include <ndis.h>
#include <lancehrd.h>
#include <lancesft.h>
#include <efilter.h>

LPWSTR FindDetectKey(VOID);

#ifdef DEBUG

//
// These defines must match the ZONE_* defines in NE2000SW.H
//
#define DBG_ERROR      1
#define DBG_WARN       2
#define DBG_FUNCTION   4
#define DBG_INIT       8
#define DBG_INTR       16
#define DBG_RCV        32
#define DBG_XMIT       64

DBGPARAM dpCurSettings = {
    TEXT("NE2000"), {
    TEXT("Errors"),TEXT("Warnings"),TEXT("Functions"),TEXT("Init"),
    TEXT("Interrupts"),TEXT("Receives"),TEXT("Transmits"),TEXT("Undefined"),
    TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),
    TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined") },
    0
};
#endif  // DEBUG

typedef struct _REG_VALUE_DESCR {
    LPWSTR val_name;
    DWORD  val_type;
    PBYTE  val_data;
} REG_VALUE_DESCR, * PREG_VALUE_DESCR;


// Jk comment out this structure as it creates a PCMCIA key which is 
// not relevant to our driver. 

// Values for [HKEY_LOCAL_MACHINE\Drivers\PCMCIA\NE2000]
//REG_VALUE_DESCR PcmKeyValues[] = {
//   (TEXT("Dll")), REG_SZ, (PBYTE)(TEXT("ndis.dll")),
//   (TEXT("Prefix")), REG_SZ, (PBYTE)(TEXT("NDS")),
//   (TEXT("Miniport")), REG_SZ, (PBYTE)(TEXT("NE2000")),
//    NULL, 0, NULL
//};

// Values for [HKEY_LOCAL_MACHINE\Comm\PCNTN4M]
// and [HKEY_LOCAL_MACHINE\Comm\PCNTN4M1]
REG_VALUE_DESCR CommKeyValues[] = {
   (TEXT("DisplayName")), REG_SZ, (PBYTE)(TEXT("PCNTN4M Compatible Ethernet Driver")),
   (TEXT("Group")), REG_SZ, (PBYTE)(TEXT("NDIS")),
   (TEXT("ImagePath")), REG_SZ, (PBYTE)(TEXT("PCNTN4M.dll")),
    NULL, 0, NULL
};

// Values for [HKEY_LOCAL_MACHINE\Comm\PCNTN4M1\Parms]
REG_VALUE_DESCR ParmKeyValues[] = {
   //(TEXT("BusNumber")), REG_DWORD, (PBYTE)0,
   (TEXT("BusType")), REG_DWORD, (PBYTE)05,
   (TEXT("Interrupt")), REG_DWORD, (PBYTE)05,
   (TEXT("IOAddress")), REG_DWORD, (PBYTE)800, // 0x0320 is the hex value 
   //(TEXT("Transceiver")), REG_DWORD, (PBYTE)3,
   //(TEXT("CardType")), REG_DWORD, (PBYTE)1,
    NULL, 0, NULL
};

// Values for [HKEY_LOCAL_MACHINE\Comm\PCNTN4M1]
REG_VALUE_DESCR LinkageKeyValues[] = {
   (TEXT("Route")), REG_MULTI_SZ, (PBYTE)(TEXT("PCNTN4M1")),
    NULL, 0, NULL
};
// Values for [HKEY_LOCAL_MACHINE\Comm\PCNTN4M1\Parms\TcpIp]
REG_VALUE_DESCR TcpIpKeyValues[] = {
	(TEXT("DefaultGateway")), REG_MULTI_SZ, (PBYTE)(TEXT("")),
	(TEXT("LLInterface")), REG_SZ, (PBYTE)(TEXT("")),
	(TEXT("UseZeroBroadcast")), REG_DWORD, (PBYTE)0,
	(TEXT("IpAddress")), REG_MULTI_SZ, (PBYTE)(TEXT("139.95.26.70")),
	(TEXT("Subnetmask")), REG_MULTI_SZ, (PBYTE)(TEXT("255.255.255.0")),
	NULL, 0, NULL
};

PREG_VALUE_DESCR Values[] = {
    //JK PcmKeyValues,
    CommKeyValues,
    CommKeyValues,
    ParmKeyValues,
    LinkageKeyValues,
	TcpIpKeyValues
};

LPWSTR KeyNames[] = {
    //(TEXT("Drivers\\PCMCIA\\PCNTN4M")), // this value is nto necesarry as it creates a PCMCIA key
    (TEXT("Comm\\PCNTN4M")),
    (TEXT("Comm\\PCNTN4M1")),
    (TEXT("Comm\\PCNTN4M1\\Parms")),
    (TEXT("Comm\\PCNTN4M\\Linkage")),
	(TEXT("Comm\\PCNTN4M1\\Parms\\TcpIp")),
	
};


//
// Standard Windows DLL entrypoint.
// Since Windows CE NDIS miniports are implemented as DLLs, a DLL entrypoint is
// needed.
//

BOOL __stdcall
DllEntry(
  HANDLE hDLL,
  DWORD dwReason,
  LPVOID lpReserved
)
{
    switch (dwReason) {
    case DLL_PROCESS_ATTACH:
		DEBUGREGISTER(hDLL);
		DEBUGMSG(ZONE_INIT, (TEXT("PCNTN4M: DLL_PROCESS_ATTACH\n")));
	break;

    case DLL_PROCESS_DETACH:
		DEBUGMSG(ZONE_INIT, (TEXT("PCNTN4M: DLL_PROCESS_DETACH\n")));
	break;
    }
    return TRUE;
}

//
// Add the specified key and its values to the registry under HKEY_LOCAL_MACHINE
//
// NOTE: This function only supports REG_MULTI_SZ strings with one item.
//
BOOL
AddKeyValues(
    LPWSTR KeyName,
    PREG_VALUE_DESCR Vals
    )
{
    DWORD Status;
    DWORD dwDisp;
    HKEY hKey;
    PREG_VALUE_DESCR pValue;
    DWORD ValLen;
    PBYTE pVal;
    DWORD dwVal;
    LPWSTR pStr;

    Status = RegCreateKeyEx(
		 HKEY_LOCAL_MACHINE,
		 KeyName,
		 0,
		 NULL,
		 REG_OPTION_NON_VOLATILE,
		 0,
		 NULL,
		 &hKey,
		 &dwDisp);
#if DBG
	DbgPrint("JK ==>AddKeyValues in Wince.c file \n");
#endif
    if (Status != ERROR_SUCCESS) {
#if DBG
	DbgPrint("JK <==AddKeyValues RETURN FALSE in Wince.c file \n");
#endif
	return FALSE;
    }

    pValue = Vals;
    while (pValue->val_name) {
	switch (pValue->val_type) {
	case REG_DWORD:
	    pVal = (PBYTE)&dwVal;
	    dwVal = (DWORD)pValue->val_data;
	    ValLen = sizeof(DWORD);
	    break;

	case REG_SZ:
	    pVal = (PBYTE)pValue->val_data;
	    ValLen = (wcslen((LPWSTR)pVal) + 1)*sizeof(WCHAR);
	    break;

	case REG_MULTI_SZ:
	    dwVal = wcslen((LPWSTR)pValue->val_data);
	    ValLen = (dwVal+2)*sizeof(WCHAR);
	    pVal = LocalAlloc(LPTR, ValLen);
	    if (pVal == NULL) {
		goto akv_fail;
	    }
	    wcscpy((LPWSTR)pVal, (LPWSTR)pValue->val_data);
	    pStr = (LPWSTR)pVal + dwVal;
	    pStr[1] = 0;
	    break;
	}
	Status = RegSetValueEx(
		     hKey,
		     pValue->val_name,
		     0,
		     pValue->val_type,
		     pVal,
		     ValLen
		     );
	if (pValue->val_type == REG_MULTI_SZ) {
	    LocalFree(pVal);
	}
akv_fail:
	if (Status != ERROR_SUCCESS) {
	    RegCloseKey(hKey);
	    return FALSE;
	}
	pValue++;
    }
    RegCloseKey(hKey);
#if DBG
	DbgPrint("JK <==AddKeyValues in Wince.c file \n");
#endif
    return TRUE;
}   // AddKeyValues


//
// Install_Driver function for the NE2000 NDIS miniport driver.
//
// This function sets up the registry keys and values required to install this
// DLL as a Windows CE plug and play driver.
//
// Input:
//
// LPWSTR lpPnpId - The device's plug and play identifier string.  An install
// function can use lpPnpId to set up a key
// HKEY_LOCAL_MACHINE\Drivers\PCMCIA\<lpPnpId> under the assumption that the
// user will continue to use the same device that generates the same plug and
// play id string.  If there is a general detection method for the card, then lpPnpId can
// be ignored and a detection function can be registered under HKEY_LOCAL_MACHINE\
// Drivers\PCMCIA\Detect.
//
// LPWSTR lpRegPath - Buffer to contain the newly installed driver's device key
// under HKEY_LOCAL_MACHINE in the registry. Windows CE will attempt to load the
// the newly installed device driver upon completion of its
// Install_Driver function.
//
// DWORD cRegPathSize - Number of bytes in lpRegPath.
//
// Returns lpRegPath if successful, NULL for failure.
//
LPWSTR
Install_Driver(
    LPWSTR lpPnpId,
    LPWSTR lpRegPath,
    DWORD  cRegPathSize
    )
{
/*
 
The following registry keys and values will be installed:

[HKEY_LOCAL_MACHINE\Comm\PCNTN4M]
   "DisplayName"="PCNTN4M Compatible Ethernet Driver"
   "Group"="NDIS"
   "ImagePath"="PCNTN4M.dll"

[HKEY_LOCAL_MACHINE\Comm\PCNTN4M\Linkage]
   "Route"=multi_sz:"NE20001"

[HKEY_LOCAL_MACHINE\Comm\PCNTN4M1]
   "DisplayName"="PCNTN4M Compatible Ethernet Driver"
   "Group"="NDIS"
   "ImagePath"="PCNTN4M.dll"

[HKEY_LOCAL_MACHINE\Comm\PCNTN4M1\Parms]
    
   "BusType"=dword:5 // 5 indicates PCMCIA as WIN CE does not support ISA ( = 1)
   "InterruptNumber"=dword:03
   "IoBaseAddress"=dword:0300
  
*/

    DWORD i;
    //jk LPWSTR lpDetectKeyName;

    DEBUGMSG(ZONE_INIT, (TEXT("PCNTN4M:Install_Driver(%s, %s, %d)\r\n"),
				lpPnpId, lpRegPath, cRegPathSize));

    for (i = 0; i < (sizeof(KeyNames)/sizeof(LPWSTR)); i++) {
		if (!AddKeyValues(KeyNames[i], Values[i])) {
			goto wid_fail;
		}
    }
     
    wcscpy(lpRegPath, KeyNames[0]);
#if DBG
	DbgPrint("JK <==Install_Driver in Wince.c file \n");
#endif
    return lpRegPath;

wid_fail:
    //
    // Clean up after ourself on failure.
    //
	DbgPrint(" FAILED To add keys to the registry KEY keynumber %d \n", i);
    for (i = 0; i < (sizeof(KeyNames)/sizeof(LPWSTR)); i++) {
	RegDeleteKey(HKEY_LOCAL_MACHINE,KeyNames[i]);
    }
	 
//jk wid_fail1:
//jk    RegDeleteKey(HKEY_LOCAL_MACHINE, lpDetectKeyName);
//jk    LocalFree(lpDetectKeyName);
    return NULL;

}

//
// Find an unused Detect key number and return the registry path name for it.
//
// Return NULL for failure
//
LPWSTR
FindDetectKey(VOID)
{
    HKEY hDetectKey;
    DWORD dwKeyNum;
    DWORD dwDisp;
    DWORD Status;
    WCHAR * pKeyName;
    LPWSTR pKeyNum;
#if DBG
	DbgPrint("JK ==>FindDetectKey in Wince.c file \n");
#endif
    pKeyName = LocalAlloc(LPTR, sizeof(WCHAR) * 255);
    if (pKeyName == NULL) {
	return NULL;
    }
    wcscpy(pKeyName, (TEXT("Drivers\\PCMCIA\\Detect\\")));
    pKeyNum = pKeyName + wcslen(pKeyName);
    
    //
    // Find a detect number and create the detect key.
    //
    for (dwKeyNum = 40; dwKeyNum < 99; dwKeyNum++) {
	wsprintf(pKeyNum, (TEXT("%02d")), dwKeyNum);
	Status = RegCreateKeyEx(
		     HKEY_LOCAL_MACHINE,
		     pKeyName,
		     0,
		     NULL,
		     REG_OPTION_NON_VOLATILE,
		     0,
		     NULL,
		     &hDetectKey,
		     &dwDisp);
	if (Status == ERROR_SUCCESS) {
	    RegCloseKey(hDetectKey);
	    if (dwDisp == REG_CREATED_NEW_KEY) {
		return pKeyName;
	    }
	}
    }
    LocalFree(pKeyName);
#if DBG
	DbgPrint("JK <==FindDetectKey in Wince.c file \n");
#endif
    return NULL;
}


