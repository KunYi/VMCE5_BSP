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
//------------------------------------------------------------------------------
//
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//  PARTICULAR PURPOSE.
//  
//------------------------------------------------------------------------------
#include <windows.h>
#include <ethdbg.h>
#include <nkintr.h>
#include <halether.h>
#include <bootarg.h>

#include "eboot.h"
#include <ceddk.h>
#include <pc.h>
#include <pci.h>

#undef TEXT
#define TEXT                                     
#define NKDbgPrintfW    EdbgOutputDebugString

const LPSTR BaseClass[] = {
    TEXT("PRE_20"),
    TEXT("MASS_STORAGE_CTLR"),
    TEXT("NETWORK_CTLR"),
    TEXT("DISPLAY_CTLR"),
    TEXT("MULTIMEDIA_DEV"),
    TEXT("MEMORY_CTLR"),
    TEXT("BRIDGE_DEV"),
    TEXT("SIMPLE_COMMS_CTLR"),
    TEXT("BASE_SYSTEM_DEV"),
    TEXT("INPUT_DEV"),
    TEXT("DOCKING_STATION"),
    TEXT("PROCESSOR"),
    TEXT("SERIAL_BUS_CTLR"),
    TEXT("UNKNOWN")
};

#define MAX_CLASS_ID (sizeof(BaseClass)/sizeof(BaseClass[0]))


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID 
PrintConfig(
    int bus,
    int device,
    int function,
    PPCI_COMMON_CONFIG pCfg
    )
{
    DWORD i;

    RETAILMSG(1, (TEXT("========================================================\r\n")));
    RETAILMSG(1, (TEXT(" Bus, Device, Function = %d, %d, %d\r\n"), bus, device, function));
    RETAILMSG(1, (TEXT(" Vendor ID, Device ID  = 0x%H, 0x%H\r\n"), pCfg->VendorID, pCfg->DeviceID));
    RETAILMSG(1, (TEXT(" Base Class, Subclass  = %d, %d => %s\r\n"), pCfg->BaseClass, pCfg->SubClass, BaseClass[min(pCfg->BaseClass, MAX_CLASS_ID - 1)]));

    if ((pCfg->HeaderType & 0x7F) == PCI_DEVICE_TYPE) {

        RETAILMSG(1, (TEXT(" Interrupt             = %d\r\n"), pCfg->u.type0.InterruptLine));
        
        for (i = 0; i < PCI_TYPE0_ADDRESSES; i++) {
            if (pCfg->u.type0.BaseAddresses[i]) {
                if (pCfg->u.type0.BaseAddresses[i] & 1) {
                    RETAILMSG(1, (TEXT(" BaseAddress[%d]        = 0x%x (I/O)\r\n"),
                        i, pCfg->u.type0.BaseAddresses[i] & 0xFFFFFFFC));
                } else {
                    RETAILMSG(1, (TEXT(" BaseAddress[%d]        = 0x%x (Memory)\r\n"),
                        i, pCfg->u.type0.BaseAddresses[i] & 0xFFFFFFE0));
                }
            }
        }
    
    } 
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BYTE
GetMaxBusNumber()
{
    PCI_SLOT_NUMBER     slotNumber;
    PCI_COMMON_CONFIG   pciConfig;
    int                 bus, device, function;
    int                 length;
    BYTE                bMaxBus = 0;

    //
    // Scan bus 0 for bridges. They'll tell us the number of buses
    //
    bus = 0;
    
    for (device = 0; device < PCI_MAX_DEVICES; device++) {
        slotNumber.u.bits.DeviceNumber = device;

        for (function = 0; function < PCI_MAX_FUNCTION; function++) {
            slotNumber.u.bits.FunctionNumber = function;

            length = PCIGetBusDataByOffset(bus, slotNumber.u.AsULONG,
                                   &pciConfig, 0, sizeof(pciConfig));

            if (length == 0 || pciConfig.VendorID == 0xFFFF) 
                break;

            if (pciConfig.BaseClass == PCI_CLASS_BRIDGE_DEV && pciConfig.SubClass == PCI_SUBCLASS_BR_PCI_TO_PCI) {
                if (pciConfig.u.type1.SubordinateBusNumber > bMaxBus) {
                    bMaxBus = pciConfig.u.type1.SubordinateBusNumber;
                }
            }

            if (function == 0 && !(pciConfig.HeaderType & 0x80)) 
                break;
            
        }
        if (length == 0)
            break;
    }

    return bMaxBus;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define IGNOREBRIDGES 1     // Set to 0 to print out basic bridge info

BOOL
PrintPCIConfig()
{
    PCI_SLOT_NUMBER     slotNumber;
    PCI_COMMON_CONFIG   pciConfig;
    int                 bus, device, function;
    int                 length;
    int                 maxbus;

    maxbus = (int) GetMaxBusNumber();

    RETAILMSG(1, (TEXT("PCI Device Configurations (%d PCI bus(es) present)...\r\n"), maxbus + 1));

    for (bus = 0; bus <= maxbus; bus++) {
        
        for (device = 0; device < PCI_MAX_DEVICES; device++) {
            slotNumber.u.bits.DeviceNumber = device;

            for (function = 0; function < PCI_MAX_FUNCTION; function++) {
                slotNumber.u.bits.FunctionNumber = function;

                length = PCIGetBusDataByOffset(bus, slotNumber.u.AsULONG,
                                       &pciConfig, 0, sizeof(pciConfig));

                if (length == 0 || pciConfig.VendorID == 0xFFFF) 
                    break;

#if IGNOREBRIDGES
                if (pciConfig.BaseClass == 6)
                    // Ignore bridges
                    break;
#endif

                PrintConfig(bus, device, function, &pciConfig);
                
                if (function == 0 && !(pciConfig.HeaderType & 0x80)) 
                    break;
                
            }
            if (length == 0)
                break;
        }

        if (length == 0 && device == 0)
            break;
    }

    RETAILMSG(1, (TEXT("========================================================\r\n")));
    return TRUE;
}



