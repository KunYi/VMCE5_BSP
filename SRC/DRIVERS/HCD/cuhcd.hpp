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
//     CUhcd.hpp
// 
// Abstract:  CUhcd implements the HCDI interface. It mostly
//            just passes requests on to other objects, which
//            do the real work.
//     
// Notes: 
//
#ifndef __CUHCD_HPP__
#define __CUHCD_HPP__

#include <new>
#include <globals.hpp>


class CUhcd;
#include "Chw.hpp"
#include "CDevice.hpp"
#include "CPipe.hpp"


// this class gets passed into the CUhcd Initialize routine
class CPhysMem;
// this class is our access point to all USB devices
//class CRootHub;

class CUhcd :public CHW, public CUHCIFrame
{
public:
    // ****************************************************
    // Public Functions for CUhcd
    // ****************************************************
    CUhcd( IN LPVOID pvUhcdPddObject,
                     IN CPhysMem * pCPhysMem,
                     IN LPCWSTR szDriverRegistryKey,
                     IN REGISTER portBase,
                     IN DWORD dwSysIntr);

    ~CUhcd();

    // These functions are called by the HCDI interface
    virtual BOOL DeviceInitialize();
    virtual void DeviceDeInitialize( void );
    virtual void SignalCheckForDoneTransfers( void ) { CUHCIFrame::SignalCheckForDoneTransfers(); };
   
    // no public variables

private:
    // ****************************************************
    // Private Functions for CUhcd
    // ****************************************************
    DWORD           m_dwSysIntr;
    REGISTER        m_portBase;
};

#endif // __CUHCD_HPP__

