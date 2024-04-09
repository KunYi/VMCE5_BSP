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
#include <x86boot.h>
#include <pc.h>
#include <x86kitl.h>

OAL_KITL_DEVICE g_kitlDevice;

void RegisterKitl (void);


BOOL InitKitlEtherArgs (OAL_KITL_ARGS *pKitlArgs)
{
    PCKITL_NIC_INFO pNic = InitKitlNIC (g_pX86Info->ucKitlIrq, g_pX86Info->dwKitlBaseAddr, g_pX86Info->ucKitlAdapterType);

    if (!pNic) {
        // can't find a ethernet controller, bail.
        return FALSE;
    }
    
    // init flags
    pKitlArgs->flags = OAL_KITL_FLAGS_ENABLED | OAL_KITL_FLAGS_EXTNAME | OAL_KITL_FLAGS_VMINI;
    if (g_pX86Info->KitlTransport & KTS_PASSIVE_MODE)
        pKitlArgs->flags |= OAL_KITL_FLAGS_PASSIVE;
    if (!g_pX86Info->fStaticIP)
        pKitlArgs->flags |= OAL_KITL_FLAGS_DHCP;

    pKitlArgs->devLoc.LogicalLoc    = pNic->dwIoBase;  // base address
    pKitlArgs->devLoc.Pin           = pNic->dwIrq? pNic->dwIrq : OAL_INTR_IRQ_UNDEFINED;
    pKitlArgs->ipAddress            = g_pX86Info->dwKitlIP;

    g_kitlDevice.type               = OAL_KITL_TYPE_ETH;
    g_kitlDevice.pDriver            = (VOID*) pNic->pDriver;

    return TRUE;
}

BOOL InitKitlSerialArgs (OAL_KITL_ARGS *pKitlArgs)
{
    DWORD dwIoBase = (1 == g_pX86Info->ucComPort)? COM2_BASE : COM1_BASE;  // base address

    // init flags
    pKitlArgs->flags = OAL_KITL_FLAGS_ENABLED | OAL_KITL_FLAGS_POLL;
    if (g_pX86Info->KitlTransport & KTS_PASSIVE_MODE)
        pKitlArgs->flags |= OAL_KITL_FLAGS_PASSIVE;

    pKitlArgs->devLoc.LogicalLoc    = dwIoBase;
    pKitlArgs->devLoc.Pin           = OAL_INTR_IRQ_UNDEFINED;
    pKitlArgs->baudRate             = CBR_115200;
    pKitlArgs->dataBits             = DATABITS_8;
    pKitlArgs->parity               = PARITY_NONE;
    pKitlArgs->stopBits             = STOPBITS_10;

    g_kitlDevice.type               = OAL_KITL_TYPE_SERIAL;
    g_kitlDevice.pDriver            = (VOID*) GetKitlSerialDriver ();
    
    return TRUE;
}

extern LPCSTR  g_oalDeviceNameRoot;
extern const UCHAR g_ucDlftKitlAdaptorType;

//-----------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------
BOOL OALKitlStart()
{
    OAL_KITL_ARGS   kitlArgs;
    BOOL            fRet = FALSE;

    memset (&kitlArgs, 0, sizeof (kitlArgs));

    if (EDBG_ADAPTER_DEFAULT == g_pX86Info->ucKitlAdapterType) {
        g_pX86Info->ucKitlAdapterType = g_ucDlftKitlAdaptorType;
    }

    // common parts
    kitlArgs.devLoc.IfcType = g_kitlDevice.ifcType
                            = InterfaceTypeUndefined;
    g_kitlDevice.name       = g_oalIoCtlPlatformType;
    
    // start the requested transport
    switch (g_pX86Info->KitlTransport & ~KTS_PASSIVE_MODE)
    {
        case KTS_SERIAL:
            fRet = InitKitlSerialArgs (&kitlArgs);
            break;

        case KTS_ETHER:
        case KTS_DEFAULT:
            fRet = InitKitlEtherArgs (&kitlArgs);
            break;
        default:
            break;
    }

    if (fRet) {

        g_kitlDevice.id = kitlArgs.devLoc.LogicalLoc;

        if (fRet = OALKitlInit (g_oalDeviceNameRoot, &kitlArgs, &g_kitlDevice)) {

            RegisterKitl ();
        }
    }
    return fRet;
}

//------------------------------------------------------------------------------
//
//  Function:  OALKitlCreateName
//
//  This function create device name from prefix and mac address (usually last
//  two bytes of MAC address used for download).
//
VOID OALKitlCreateName(CHAR *pPrefix, UINT16 mac[3], CHAR *pBuffer)
{
    if (!x86KitlCreateName (pPrefix, mac, pBuffer)) {
        // not much we can do here since it doesn't have a return value.
        // Just copy the prefix to buffer. (message had displayed already,
        // don't need to do it again)
        strncpy (pBuffer, pPrefix, OAL_KITL_ID_SIZE-1);
    }

    // update the mac address and device name
    memcpy (g_pX86Info->wMac, mac, sizeof (g_pX86Info->wMac));
    strcpy (g_pX86Info->szDeviceName, pBuffer);
    
    RETAILMSG (1, (L"OALKitlCreateName: Using Device Name '%a'\r\n", pBuffer));
}




