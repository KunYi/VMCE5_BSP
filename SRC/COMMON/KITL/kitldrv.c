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
#include <x86kitl.h>

//------------------------------------------------------------------------------
// Prototypes for AM79C970

BOOL   AM79C970Init(UINT8 *pAddress, UINT32 offset, UINT16 mac[3]);
BOOL   AM79C970InitDMABuffer(UINT32 address, UINT32 size);
UINT16 AM79C970SendFrame(UINT8 *pbData, UINT32 length);
UINT16 AM79C970GetFrame(UINT8 *pbData, UINT16 *pLength);
VOID   AM79C970EnableInts();
VOID   AM79C970DisableInts();

#define OAL_ETHDRV_AM79C970     { \
    AM79C970Init, \
	AM79C970InitDMABuffer, \
	NULL, \
	AM79C970SendFrame, AM79C970GetFrame, \
	AM79C970EnableInts, AM79C970DisableInts, \
    NULL, NULL, \
    NULL, NULL, \
}

static OAL_KITL_ETH_DRIVER DrvNE2k  = OAL_ETHDRV_NE2000;     // NE2000 
static OAL_KITL_ETH_DRIVER DrvRTL   = OAL_ETHDRV_RTL8139;    // RTL8139
static OAL_KITL_ETH_DRIVER DrvDP    = OAL_ETHDRV_DP83815;    // DP83815
static OAL_KITL_ETH_DRIVER DrvRndis = OAL_ETHDRV_RNDIS;      // RNDIS
static OAL_KITL_ETH_DRIVER Drv3C90  = OAL_ETHDRV_3C90X;      // 3C90X
static OAL_KITL_ETH_DRIVER DrvAM70	= OAL_ETHDRV_AM79C970;   // AM79C970
static OAL_KITL_ETH_DRIVER DrvAM73	= OAL_ETHDRV_AM79C973;   // AM79C973

#define EDBG_ADAPTER_AM79C970 7
#define EDBG_ADAPTER_AM79C973 8

const SUPPORTED_NIC g_NicSupported []=
{
//   VenId   DevId   UpperMAC      Type              Name   Drivers
//  ---------------------------------------------------------------------------------
    {0x0000, 0x0000, 0x004033, EDBG_ADAPTER_NE2000,  "AD", &DrvNE2k  }, /* Addtron */\
    {0x1050, 0x0940, 0x004005, EDBG_ADAPTER_NE2000,  "LS", &DrvNE2k  }, /* LinkSys */\
    {0x1050, 0x0940, 0x002078, EDBG_ADAPTER_NE2000,  "LS", &DrvNE2k  }, /* LinkSys */\
    {0x10EC, 0x8029, 0x00C0F0, EDBG_ADAPTER_NE2000,  "KS", &DrvNE2k  }, /* Kingston */\
    {0x10EC, 0x8129, 0x000000, EDBG_ADAPTER_RTL8139, "RT", &DrvRTL   }, /* RealTek */\
    {0x10EC, 0x8139, 0x00900B, EDBG_ADAPTER_RTL8139, "RT", &DrvRTL   }, /* RealTek  */\
    {0x10EC, 0x8139, 0x00D0C9, EDBG_ADAPTER_RTL8139, "RT", &DrvRTL   }, /* RealTek */\
    {0x10EC, 0x8139, 0x00E04C, EDBG_ADAPTER_RTL8139, "RT", &DrvRTL   }, /* RealTek */\
    {0x1186, 0x1300, 0x0050BA, EDBG_ADAPTER_RTL8139, "DL", &DrvRTL   }, /* D-Link */\
    {0x100B, 0x0020, 0x00A0CC, EDBG_ADAPTER_DP83815, "NG", &DrvDP    }, /* Netgear */\
    {0x10B7, 0x9050, 0x006008, EDBG_ADAPTER_3C90X,   "3C", &Drv3C90  }, /* 3Com */\
    {0x10B7, 0x9200, 0x000476, EDBG_ADAPTER_3C90X,   "3C", &Drv3C90  }, /* 3Com */
    {0x10b5, 0x9054, 0x00800f, EDBG_USB_RNDIS,       "NC", &DrvRndis }, /* NetChip */
    {0x1022, 0x2000, 0x000C29, EDBG_ADAPTER_AM79C970,"AM", &DrvAM70 },	/* AMD */
    {0x1022, 0x2001, 0x000C29, EDBG_ADAPTER_AM79C973,"AM", &DrvAM73 },	/* AMD */
};

const int g_nNumNicSupported = sizeof (g_NicSupported) / sizeof (g_NicSupported[0]);

