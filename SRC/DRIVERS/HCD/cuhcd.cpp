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
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Module Name:
//     cuhcd.cpp
// Abstract:
//     This file contains the CUhcd object, which is the main entry
//     point for all HCDI calls by USBD
//
// Notes:
//


#include "cuhcd.hpp"
#include "cpipe.hpp"



//BOOL g_fPowerUpFlag = FALSE;
//BOOL g_fPowerResuming = FALSE;

// ****************************************************************
// PUBLIC FUNCTIONS
// ****************************************************************

// ******************************************************************
CUhcd::CUhcd( IN LPVOID pvUhcdPddObject,
                        IN CPhysMem * pCPhysMem,
                        IN LPCWSTR, // szDriverRegistryKey ignored for now
                        IN REGISTER portBase,
                        IN DWORD dwSysIntr)
:CHW( portBase,dwSysIntr,pCPhysMem,pvUhcdPddObject )
,CUHCIFrame(pCPhysMem )
// Purpose: Initialize variables associated with this class
//
// Parameters: None
//
// Returns: Nothing (Constructor can NOT fail!)
//
// Notes: *All* initialization which could possibly fail should be done
//        via the Initialize() routine, which is called right after
//        the constructor
// ******************************************************************
{
    DEBUGMSG(ZONE_HCD && ZONE_VERBOSE, (TEXT("+CUhcd::CUhcd\n")));
    m_dwSysIntr = dwSysIntr;
    DEBUGMSG(ZONE_HCD && ZONE_VERBOSE, (TEXT("-CUhcd::CUhcd\n")));
}

// ******************************************************************
CUhcd::~CUhcd()
//
// Purpose: Destroy all memory and objects associated with this class
//
// Parameters: None
//
// Returns: Nothing
//
// Notes:
// ******************************************************************
{
    DEBUGMSG(ZONE_HCD && ZONE_VERBOSE, (TEXT("+CUhcd::~CUhcd\n")));

    // make the API set inaccessible
    CRootHub *pRoot = SetRootHub(NULL);
    // signal root hub to close
    if ( pRoot ) {
        pRoot->HandleDetach();
        delete pRoot;
    }
    CHW::StopHostController();

    DEBUGMSG(ZONE_HCD && ZONE_VERBOSE, (TEXT("-CUhcd::~CUhcd\n")));
}
void CUhcd::DeviceDeInitialize( void )
{
    DEBUGMSG(ZONE_HCD && ZONE_VERBOSE, (TEXT("+CUhcd::DeInitialize\n")));

    CHW::StopHostController();

    // make the API set inaccessible
    CRootHub *pRoot = SetRootHub(NULL);
    // signal root hub to close
    if ( pRoot ) {
        pRoot->HandleDetach();
        delete pRoot;
    }
    // this is safe because by now all clients have been unloaded
    //DeleteCriticalSection ( &m_csHCLock );

    CDeviceGlobal::DeInitialize();
    CUHCIFrame::DeInitialize();
    CHW::DeInitialize();

    DEBUGMSG(ZONE_HCD && ZONE_VERBOSE, (TEXT("-CUhcd::DeInitialize\n")));

}

// ******************************************************************
BOOL CUhcd::DeviceInitialize()
//
// Purpose: Set up the Host Controller hardware, associated data structures,
//          and threads so that schedule processing can begin.
//
// Parameters: pvUhcdPddObject - pointer to the PDD object for this driver
//
//             pCPhysMem - pointer to class for managing physical memory
//
//             szDriverRegistryKey - unused ?
//
//             portBase - base address for USB registers
//
//             dwSysIntr - interrupt identifier for USB interrupts
//
// Returns: TRUE - if initializes successfully and is ready to process
//                 the schedule
//          FALSE - if setup fails
//
// Notes: This function is called by right after the constructor.
//        It is the starting point for all initialization.
//
//        This needs to be implemented for HCDI
// ******************************************************************
{
    DEBUGMSG(ZONE_INIT,(TEXT("+CUhcd::Initialize. Entry\r\n")));


    // All Initialize routines must be called, so we can't write
    // if ( !CDevice::Initialize() || !CPipe::Initialize() etc )
    // due to short circuit eval.
    {
        BOOL fCDeviceInitOK = CDeviceGlobal::Initialize(this);
        BOOL fCHWInitOK = CHW::Initialize( );
        BOOL fCPipeInitOK = CUHCIFrame::Initialize(this);

        if ( !fCPipeInitOK || !fCPipeInitOK || !fCHWInitOK ) {
            DEBUGMSG(ZONE_ERROR, (TEXT("-CUhcd::Initialize. Error - could not initialize device/pipe/hw classes\n")));
            return FALSE;
        }
    }

    // set up the root hub object
    {
        USB_DEVICE_INFO deviceInfo;
        USB_HUB_DESCRIPTOR usbHubDescriptor;

        deviceInfo.dwCount = sizeof( USB_DEVICE_INFO );
        deviceInfo.lpConfigs = NULL;
        deviceInfo.lpActiveConfig = NULL;
        deviceInfo.Descriptor.bLength = sizeof( USB_DEVICE_DESCRIPTOR );
        deviceInfo.Descriptor.bDescriptorType = USB_DEVICE_DESCRIPTOR_TYPE;
        deviceInfo.Descriptor.bcdUSB = 0x110; // USB spec 1.10
        deviceInfo.Descriptor.bDeviceClass = USB_DEVICE_CLASS_HUB;
        deviceInfo.Descriptor.bDeviceSubClass = 0xff;
        deviceInfo.Descriptor.bDeviceProtocol = 0xff;
        deviceInfo.Descriptor.bMaxPacketSize0 = 0;
        deviceInfo.Descriptor.bNumConfigurations = 0;

        usbHubDescriptor.bDescriptorType = USB_HUB_DESCRIPTOR_TYPE;
        usbHubDescriptor.bDescriptorLength = USB_HUB_DESCRIPTOR_MINIMUM_SIZE;
        usbHubDescriptor.bNumberOfPorts = HCD_NUM_ROOT_HUB_PORTS;
        usbHubDescriptor.wHubCharacteristics = USB_HUB_CHARACTERISTIC_NO_POWER_SWITCHING |
                                               USB_HUB_CHARACTERISTIC_NOT_PART_OF_COMPOUND_DEVICE |
                                               USB_HUB_CHARACTERISTIC_NO_OVER_CURRENT_PROTECTION;
        usbHubDescriptor.bPowerOnToPowerGood = 0;
        usbHubDescriptor.bHubControlCurrent = 0;
        DEBUGCHK( HCD_NUM_ROOT_HUB_PORTS < 8 );
        usbHubDescriptor.bRemoveAndPowerMask[0] = 0; // all devices on hub are removable
        usbHubDescriptor.bRemoveAndPowerMask[1] = 0xFF; // must be 0xFF, USB spec 1.1, table 11-8

        // FALSE indicates root hub is not low speed
        // (though, this is ignored for hubs anyway)
        SetRootHub( new CRootHub( deviceInfo, FALSE, usbHubDescriptor,this ));
    }
    if ( !GetRootHub() ) {
        DEBUGMSG( ZONE_ERROR, (TEXT("-CUhcd::Initialize - unable to create root hub object\n")) );
        return FALSE;
    }

    // Signal root hub to start processing port changes
    // The root hub doesn't have any pipes, so we pass NULL as the
    // endpoint0 pipe
    if ( !GetRootHub()->EnterOperationalState( NULL ) ) {
        DEBUGMSG(ZONE_ERROR, (TEXT("-CUhcd::Initialize. Error initializing root hub\n")));
        return FALSE;
    }
    // Start processing frames
    CHW::EnterOperationalState();

    DEBUGMSG(ZONE_INIT,(TEXT("-CUhcd::Initialize. Success!!\r\n")));
    return TRUE;
}

CHcd * CreateHCDObject(IN LPVOID pvUhcdPddObject,
                     IN CPhysMem * pCPhysMem,
                     IN LPCWSTR szDriverRegistryKey,
                     IN REGISTER portBase,
                     IN DWORD dwSysIntr)
{
    return new CUhcd (pvUhcdPddObject, pCPhysMem,szDriverRegistryKey,portBase,dwSysIntr);
}


