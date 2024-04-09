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
#include <oal_intr.h>
#include <PCIReg.h>
#include <x86kitl.h>

extern PCI_REG_INFO     g_KitlPCIInfo;
extern PCKITL_NIC_INFO  g_pKitlPCINic;         // non-null only if it's PCI. Used in RegisterKitl

BOOL PCIReadBARs (PPCI_REG_INFO pInfo);
ULONG PCIReadBusData (IN ULONG BusNumber,
              IN ULONG DeviceNumber,
              IN ULONG FunctionNumber,
              OUT PVOID Buffer,
              IN ULONG Offset,
              IN ULONG Length
              );

void RegisterKitl (void)
{
    if (g_pKitlPCINic) {
        
        DWORD dwSysIntr = (g_pKitlPCINic->dwIrq)? OALIntrTranslateIrq (g_pKitlPCINic->dwIrq) : KITL_SYSINTR_NOINTR;
        PCI_COMMON_CONFIG pciConfig = g_pKitlPCINic->pciConfig;
        
        // Initialize KITL info
        PCIInitInfo (L"Drivers\\BuiltIn\\PCI\\Instance\\KITL", 
            g_pKitlPCINic->dwBus,
            g_pKitlPCINic->dwDevice,
            g_pKitlPCINic->dwFunction,
            dwSysIntr,
            &pciConfig,
            &g_KitlPCIInfo);

        // Fill out memory and I/O resource info
        PCIReadBARs (&g_KitlPCIInfo);
    }
}

