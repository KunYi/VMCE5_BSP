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
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:
    main.c

Abstract:
    Ethernet boot loader main module. This file contains the C main
    for the boot loader.    NOTE: The firmware "entry" point (the real
    entry point is _EntryPoint in init assembler file.

    The Windows CE boot loader is the code that is executed on a Windows CE
    development system at power-on reset and loads the Windows CE
    operating system. The boot loader also provides code that monitors
    the behavior of a Windows CE platform between the time the boot loader
    starts running and the time the full operating system debugger is 
    available. Windows CE OEMs are supplied with sample boot loader code 
    that runs on a particular development platform and CPU.

Functions:


Notes: 

--*/

#include <windows.h>
#include <nkintr.h>
#include <bootarg.h>
#include <pc.h>
#include <ceddk.h>
#include <pehdr.h>
#include <romldr.h>
#include <blcommon.h>
#include "eboot.h"
#include <kitlprot.h>
#include "x86kitl.h"
#include <pci.h>

#define PLATFORM_STRING             "CEPC"

#define BOOT_ARG_PTR_LOCATION_NP    0x001FFFFC
#define BOOT_ARG_LOCATION_NP        0x001FFF00

#define ETHDMA_BUFFER_BASE      0x00200000
#define ETHDMA_BUFFER_SIZE      0x00020000

// Can be used to turn on debug zones in eboot.lib and smc9000.lib functions
DWORD EdbgDebugZone;
// FLASH backup support
BOOL g_bDownloadImage = TRUE;
// Multi-XIP
#define FLASH_BIN_START 0  // FLASH Offset.
MultiBINInfo g_BINRegionInfo;
DWORD g_dwMinImageStart;

const OAL_KITL_ETH_DRIVER *g_pEdbgDriver;

USHORT wLocalMAC[3];

BOOL PrintPCIConfig();

// For NAND FLASH
#ifdef NAND_FLASH
#include <fmd.h>
BOOL FlashInit(BOOT_ARGS * pDriverGlobals);
BOOL ReadNANDStoredBootArgs(BOOT_ARGS *pBootArgs);
BOOL WriteNANDStoredBootArgs(BOOT_ARGS * pBootArgs);
BOOL ReadNANDFlash(DWORD dwStartAddr, LPBYTE pBuffer, DWORD dwLength);
BOOL WriteAndVerifyFlash (DWORD dwStartAddr, LPBYTE pData, DWORD dwLength);
BOOL CreateBINFSPartition(DWORD dwStart, DWORD dwLegnth);
BOOL CreateExtendedPartition();
BOOL OEMWriteFlash2 (DWORD dwStartAddr, LPBYTE pData, DWORD dwLength);
#endif
PVOID GetKernelExtPointer(DWORD dwRegionStart, DWORD dwRegionLength);

// file globals
static BOOT_ARGS *pBootArgs;    // bootarg pointer
static EDBG_ADDR MyAddr;        // CEPC address
static BOOL AskUser (EDBG_ADDR *pMyAddr, DWORD *pdwSubnetMask);

typedef void (*PFN_LAUNCH)();
extern PFN_OEMVERIFYMEMORY g_pOEMVerifyMemory;
DWORD   g_dwStartAddr, g_dwLength, g_dwOffset;
void DrawProgress (int Percent, BOOL fBackground);

//
// stub variables to keep linker happy
//
BOOL g_fPostInit;
DWORD CurMSec;


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
SpinForever(void)
{
    KITLOutputDebugString("SpinForever...\r\n");
    while(1);
}

//------------------------------------------------------------------------------
//
//  Function Name:  OEMVerifyMemory (DWORD dwStartAddr, DWORD dwLength)
//
//  Description:    This function verifies that the passed in memory range
//                  falls within a valid address. It also sets the global
//                  Start and Length values - this should be moved elsewhere.
//
//------------------------------------------------------------------------------

BOOL OEMVerifyMemory (DWORD dwStartAddr, DWORD dwLength)
{
    BOOL    rc;                 // return code
    DWORD   Addr1;              // starting address
    DWORD   Addr2;              // ending   address

    // Setup address range for comparison

    Addr1 = dwStartAddr;
    Addr2 = Addr1 + (dwLength - 1);

    KITLOutputDebugString( "****** OEMVerifyMemory Checking Range [ 0x%x ==> 0x%x ]\r\n", Addr1, Addr2 );

    // Validate each range

    // since we don't have pTOC information yet and we don't know exactly how much memory is on the CEPC,
    // We'll just skip the end address testing.
    // if( (Addr1 >= RAM_START) && (Addr2 <= RAM_END) )
    if (Addr1 >= RAM_START)
    {
        KITLOutputDebugString("****** RAM Address ****** \r\n\r\n");

        rc = TRUE;
    }
    else
    {
        KITLOutputDebugString("****** OEMVerifyMemory FAILED - Invalid Memory Area ****** \r\n\r\n");

        rc = FALSE;
    }

    // If the range is verified set the start and length globals

    if( rc == TRUE )
    {
        // Update progress 

        DrawProgress (100, TRUE); 

        g_dwStartAddr = dwStartAddr;
        g_dwLength = dwLength;
    }

    // Indicate status

    return( rc );
}

BOOL OEMDebugInit (void)
{
    // Initialize the monitor port (for development system)
    // This should be done first since otherwise we can not
    // print any message onto the monitor.
    
    OEMInitDebugSerial();

    // Set the pointer to our VerifyMemory routine

    g_pOEMVerifyMemory = OEMVerifyMemory;

    // Indicate success

    return( TRUE );
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

BOOL OEMPlatformInit (void)
{
    extern void BootUp (void);
    PCKITL_NIC_INFO pNicInfo;

    KITLOutputDebugString("Microsoft Windows CE Ethernet Bootloader %d.%d for CE/PC (%s)\n",
                          EBOOT_VERSION_MAJOR,EBOOT_VERSION_MINOR, __DATE__);

    // init PCI config mechanism
    PCIInitConfigMechanism (1);
    
    //
    // Get pointer to Boot Args...
    //
    pBootArgs = (BOOT_ARGS *) ((ULONG)(*(PBYTE *)BOOT_ARG_PTR_LOCATION_NP));
    KITLOutputDebugString("Boot Args @ 0x%x and  ucLoaderFlags is %x \r\n", pBootArgs,pBootArgs->ucLoaderFlags);

    pBootArgs->dwEBootAddr = (DWORD) BootUp;
#ifdef NAND_FLASH
    if (!FlashInit(pBootArgs)) // If we do not find NAND. ignore the backup flag.
        pBootArgs->ucLoaderFlags &= ~LDRFL_FLASH_BACKUP;
    
    if ((pBootArgs->ucLoaderFlags & LDRFL_USE_EDBG)==0  && 
            (pBootArgs->ucLoaderFlags & LDRFL_FLASH_BACKUP)!=0 ) {
        DWORD dwLaunchAddr;
        UCHAR ucLoaderFlags=pBootArgs->ucLoaderFlags;
        KITLOutputDebugString("No Ethernet Support. Boot From RAM (or Backup)!\r\n");
        pBootArgs->ucLoaderFlags |= LDRFL_JUMPIMG;
        if ((pBootArgs->ucLoaderFlags & LDRFL_FLASH_BACKUP)!=0 &&
                ReadNANDStoredBootArgs(pBootArgs)){ 
            KITLOutputDebugString("Loading (Flash=0x%x  RAM=0x%x  Length=0x%x) (please wait).", pBootArgs->dwImgStoreAddr, pBootArgs->dwImgLoadAddr, pBootArgs->dwImgLength);
            if (!ReadNANDFlash(pBootArgs->dwImgStoreAddr, (LPBYTE)pBootArgs->dwImgLoadAddr, pBootArgs->dwImgLength))
            {
                KITLOutputDebugString("ERROR: Reading RAM image from flash failed.\r\n");
                return(FALSE);
            }
        }
        // No EDBG support on this condition.
        pBootArgs->ucLoaderFlags &= ~LDRFL_USE_EDBG; 
        pBootArgs->KitlTransport = KTS_NONE;
        KITLOutputDebugString("No Ethernet Support. Jump to %x !\r\n",pBootArgs->dwLaunchAddr);
        dwLaunchAddr = pBootArgs->dwLaunchAddr;
        ((PFN_LAUNCH)(dwLaunchAddr))();
        // never reached
        SpinForever ();
        return TRUE;
    }
#endif
    pBootArgs->ucLoaderFlags &= LDRFL_FLASH_BACKUP ;

    //
    // What PCI hardware is available?
    //
    PrintPCIConfig();

    if (EDBG_ADAPTER_DEFAULT == pBootArgs->ucEdbgAdapterType) {
        pBootArgs->ucEdbgAdapterType = EDBG_ADAPTER_NE2000;
    }

    pNicInfo = InitKitlNIC (pBootArgs->ucEdbgIRQ, pBootArgs->dwEdbgBaseAddr, pBootArgs->ucEdbgAdapterType);

    if (!pNicInfo) {
        KITLOutputDebugString ("Unable to find NIC card, spin forever\r\n");
        return FALSE;
    }

    g_pEdbgDriver = pNicInfo->pDriver;

    // update bootarg
    pBootArgs->ucEdbgAdapterType = (UCHAR) pNicInfo->dwType;
    pBootArgs->ucEdbgIRQ         = (UCHAR) pNicInfo->dwIrq;

    // special case for RNDIS -- set base to 0 to force USB to scan PCI
    pBootArgs->dwEdbgBaseAddr    = (EDBG_USB_RNDIS != pNicInfo->dwType)? pNicInfo->dwIoBase : 0;

    //
    // Initialize NIC DMA buffer, if required.
    //
    if (g_pEdbgDriver->pfnInitDmaBuffer
        && !g_pEdbgDriver->pfnInitDmaBuffer (ETHDMA_BUFFER_BASE, ETHDMA_BUFFER_SIZE)) {

        KITLOutputDebugString("ERROR: Failed to initialize Ethernet controller DMA buffer.\r\n");
        return FALSE;
    }

    // Call driver specific initialization.
    //
    if (!g_pEdbgDriver->pfnInit ((BYTE *) pBootArgs->dwEdbgBaseAddr, 1, MyAddr.wMAC)) {
        KITLOutputDebugString("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\r\n");
        KITLOutputDebugString("---------------------------------------------\r\n");
        KITLOutputDebugString(" Failed to initialize Ethernet board!\r\n");
        KITLOutputDebugString(" Please check that the Ethernet card is\r\n");
        KITLOutputDebugString(" properly installed and configured.\r\n");
        KITLOutputDebugString("---------------------------------------------\r\n");
        KITLOutputDebugString("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\r\n");
        return FALSE;
    }


    // Specific for Net-Chip Card.
    //
    if (EDBG_USB_RNDIS == pNicInfo->dwType) { // update Interrupt IRQ info
        RndisGetPCICard (&pBootArgs->dwEdbgBaseAddr, &pBootArgs->ucEdbgIRQ);
        KITLOutputDebugString(" Rndis BaseAddr=%x,Interrupt = %d !\r\n",pBootArgs->dwEdbgBaseAddr,pBootArgs->ucEdbgIRQ);
    }

    // Display the MAC address returned by the EDBG driver init function.
    //
    KITLOutputDebugString("Returned MAC Address:%B:%B:%B:%B:%B:%B\r\n",
                           MyAddr.wMAC[0] & 0x00FF, MyAddr.wMAC[0] >> 8,
                           MyAddr.wMAC[1] & 0x00FF, MyAddr.wMAC[1] >> 8,
                           MyAddr.wMAC[2] & 0x00FF, MyAddr.wMAC[2] >> 8);

    return(TRUE);
}

DWORD OEMPreDownload (void)
{
    DWORD dwSubnetMask, dwBootFlags = 0;
    DWORD DHCPLeaseTime = DEFAULT_DHCP_LEASE, *pDHCPLeaseTime = &DHCPLeaseTime;
    char szDeviceName[EDBG_MAX_DEV_NAMELEN], szNameRoot[EDBG_MAX_DEV_NAMELEN];
    BOOL fGotJumpImg = FALSE;

    //
    // set bootloader capability
    //
    // support passive-kitl
    KITLOutputDebugString("OEMPreDownload  ucLoaderFlags is %x \r\n",pBootArgs->ucLoaderFlags);
    dwBootFlags = EDBG_CAPS_PASSIVEKITL;

    // check if this is a cold boot. Force download if so.
    if (BOOTARG_SIG != pBootArgs->dwEBootFlag) {
        dwBootFlags |= EDBG_BOOTFLAG_FORCE_DOWNLOAD;
    }

    // Create device name based on Ethernet address
    // If a custom device name was specified on the loadcepc command line,
    // use it.
    //
    if ((pBootArgs->dwVersionSig == BOOT_ARG_VERSION_SIG) && 
        (pBootArgs->szDeviceNameRoot[0] != '\0'))
    {
        strncpy(szNameRoot, pBootArgs->szDeviceNameRoot, (sizeof szNameRoot)/(sizeof szNameRoot[0]));

        // Ensure that the string is null terminated 
        szNameRoot[((sizeof szNameRoot)/(sizeof szNameRoot[0])) - 1] = '\0';
    }
    else
    {
        strcpy(szNameRoot, PLATFORM_STRING);
        if (pBootArgs->dwVersionSig == BOOT_ARG_VERSION_SIG)
            strncpy(pBootArgs->szDeviceNameRoot, PLATFORM_STRING, EDBG_MAX_DEV_NAMELEN);
    }

    x86KitlCreateName (szNameRoot, MyAddr.wMAC, szDeviceName);
    KITLOutputDebugString("Using device name: %s\n", szDeviceName);

    // Assign a default subnet mask.
    dwSubnetMask = ntohl(0xffff0000);
    
    // IP address may have been passed in on loadcepc cmd line
    // give user a chance to enter IP address if not
    if (!pBootArgs->EdbgAddr.dwIP && AskUser (&MyAddr, &dwSubnetMask)) {
        // user entered static IP
        pBootArgs->EdbgAddr.dwIP = MyAddr.dwIP;
    }

    // use the static IP address if we have one
    if (pBootArgs->EdbgAddr.dwIP) {
        KITLOutputDebugString("Using static IP address: %X\r\n",pBootArgs->EdbgAddr.dwIP);
        MyAddr.dwIP = pBootArgs->EdbgAddr.dwIP;
        pDHCPLeaseTime = NULL;  // Skip DHCP
        pBootArgs->EdbgFlags |= EDBG_FLAGS_STATIC_IP;
    }

    // initialize TFTP transport
    if (!EbootInitEtherTransport (&MyAddr, &dwSubnetMask, &fGotJumpImg, pDHCPLeaseTime,
        EBOOT_VERSION_MAJOR, EBOOT_VERSION_MINOR, PLATFORM_STRING, szDeviceName, EDBG_CPU_i486, dwBootFlags)) {
        return BL_ERROR;
    }

    // update BOOTARG with the info we got so far
    memcpy(&pBootArgs->EdbgAddr,&MyAddr,sizeof(MyAddr));
    pBootArgs->ucLoaderFlags |= LDRFL_USE_EDBG | LDRFL_ADDR_VALID | LDRFL_JUMPIMG;
    pBootArgs->DHCPLeaseTime = DHCPLeaseTime;

    g_bDownloadImage=(fGotJumpImg?FALSE:TRUE);
    return fGotJumpImg? BL_JUMP : BL_DOWNLOAD;
}

void OEMLaunch (DWORD dwImageStart, DWORD dwImageLength, DWORD dwLaunchAddr, const ROMHDR *pRomHdr)
{
    EDBG_OS_CONFIG_DATA *pCfgData;
    EDBG_ADDR EshellHostAddr;

    KITLOutputDebugString("OEMLaunch   ucLoaderFlags is %x \r\n",pBootArgs->ucLoaderFlags);
    // 
    if(pBootArgs->ucEdbgAdapterType == EDBG_ADAPTER_DP83815)
    {
        DP83815Disable();  // we have to turn off the DP83815 free-running state machines before jumping to new image.
    }

    // Pretend we're 100% done with download
    DrawProgress (100, FALSE);

    memset (&EshellHostAddr, 0, sizeof (EshellHostAddr));

    if (!dwLaunchAddr) {
        dwLaunchAddr = pBootArgs->dwLaunchAddr;
    } else {
        pBootArgs->dwLaunchAddr = dwLaunchAddr;
    }

    // wait for the jump command from eshell unless user specify jump directly
    KITLOutputDebugString ("Download successful! Jumping to image at %Xh...\r\n", dwLaunchAddr);
    if (!(pCfgData = EbootWaitForHostConnect (&MyAddr, &EshellHostAddr))) {
        KITLOutputDebugString ("EbootWaitForHostConenct failed, spin forever\r\n");
        SpinForever();
        return;
    }

    // update service information
    if (pCfgData->Flags & EDBG_FL_DBGMSG) {
        KITLOutputDebugString("Enabling debug messages over Ethernet, IP: %s, port:%u\n",
                              inet_ntoa(pCfgData->DbgMsgIPAddr),ntohs(pCfgData->DbgMsgPort));
        memcpy(&pBootArgs->DbgHostAddr.wMAC,&EshellHostAddr.wMAC,6);
        pBootArgs->DbgHostAddr.dwIP  = pCfgData->DbgMsgIPAddr;
        pBootArgs->DbgHostAddr.wPort = pCfgData->DbgMsgPort;
    }
    if (pCfgData->Flags & EDBG_FL_PPSH) {
        KITLOutputDebugString("Enabling CESH over Ethernet,           IP: %s, port:%u\n",
                              inet_ntoa(pCfgData->PpshIPAddr),ntohs(pCfgData->PpshPort));
        memcpy(&pBootArgs->CeshHostAddr.wMAC,&EshellHostAddr.wMAC,6);
        pBootArgs->CeshHostAddr.dwIP  = pCfgData->PpshIPAddr;
        pBootArgs->CeshHostAddr.wPort = pCfgData->PpshPort;
    }
    if (pCfgData->Flags & EDBG_FL_KDBG) {
        KITLOutputDebugString("Enabling KDBG over Ethernet,           IP: %s, port:%u\n",
                              inet_ntoa(pCfgData->KdbgIPAddr),ntohs(pCfgData->KdbgPort));
        memcpy(&pBootArgs->KdbgHostAddr.wMAC,&EshellHostAddr.wMAC,6);
        pBootArgs->KdbgHostAddr.dwIP  = pCfgData->KdbgIPAddr;
        pBootArgs->KdbgHostAddr.wPort = pCfgData->KdbgPort;
    }
    pBootArgs->ucEshellFlags = pCfgData->Flags;
    pBootArgs->dwEBootFlag = BOOTARG_SIG;
    pBootArgs->KitlTransport = pCfgData->KitlTransport;
    memcpy (&pBootArgs->EshellHostAddr, &EshellHostAddr, sizeof(EDBG_ADDR));

#ifdef NAND_FLASH
    // If the user requested a RAM image be stored in flash, do so now.  For multiple RAM BIN files, we need to map
    // its RAM address to a flash address - the image base address offset in RAM is maintained in flash.
    //
    KITLOutputDebugString("NAND_FLASH with g_bDownloadImage=%d ,  pBootArgs->ucLoaderFlags=%x \r\n",
        g_bDownloadImage,pBootArgs->ucLoaderFlags);
    if (g_bDownloadImage && ((pBootArgs->ucLoaderFlags & LDRFL_FLASH_BACKUP)!=0))
    {
        BYTE nCount;
        DWORD dwFlashAddress;
        DWORD dwNumExts;
        PXIPCHAIN_SUMMARY pChainInfo = NULL;
        EXTENSION *pExt = NULL;
        DWORD dwImgStoreAddr=0;
        DWORD dwFormatLength = 0;

        KITLOutputDebugString("NAND_FLASH do image backup\r\n");

        // Look in the kernel regions extension area for a multi-BIN extension descriptor.  This region, if found, details the number, start, and size of each XIP region.
        for (nCount = 0, dwNumExts = 0 ; (nCount < g_BINRegionInfo.dwNumRegions) && !pChainInfo ; nCount++)
        {
            // Does this region contain nk.exe and an extension pointer?
            if ((pExt = (EXTENSION *)GetKernelExtPointer(g_BINRegionInfo.Region[nCount].dwRegionStart, g_BINRegionInfo.Region[nCount].dwRegionLength)) != NULL)
            {
                // If there is an extension pointer region, walk it until the end.
                while (pExt)
                {
                    DWORD dwBaseAddr = g_BINRegionInfo.Region[nCount].dwRegionStart;
                    pExt = (EXTENSION *)OEMMapMemAddr(dwBaseAddr, (DWORD)pExt);
                    if ((pExt->type == 0) && !strcmp(pExt->name, "chain information"))
                    {
                        pChainInfo = (PXIPCHAIN_SUMMARY) OEMMapMemAddr(dwBaseAddr, (DWORD)pExt->pdata);
                        dwNumExts = (pExt->length / sizeof(XIPCHAIN_SUMMARY));
                        KITLOutputDebugString("Found chain information (pChainInfo=0x%x  Extensions=0x%x).\r\n", (DWORD)pChainInfo, dwNumExts);
                        break;
                    }
                    pExt = (EXTENSION *)pExt->pNextExt;
                }
            }
        }

        // If we're downloading all the files in a multi-region image, first format the flash and write out logical sector numbers.
        if (pChainInfo && dwNumExts == g_BINRegionInfo.dwNumRegions)
        {
            // Skip the first entry - this is the chain file.
            for (nCount = 0, dwFormatLength = 0 ; nCount < dwNumExts ; nCount++)
            {
                // Handle both cases where the image is built for flash or for RAM.
                dwFormatLength += (pChainInfo + nCount)->dwMaxLength;
            }
        }
        else if (dwLaunchAddr)  // If we're downloading a single image that contains the kernel (signified by the fact that blcommon gave us a launch address)
        {                       // then format the flash and write logical sector numbers.  Note that we don't want to format flash when downloading a single image of a multi-image build.
            dwFormatLength = dwImageLength;
        }

        if (dwFormatLength)
        {
            KITLOutputDebugString("Create Partition  flash blocks [0x%x through 0x%x] - this is the maximum size of all BIN regions.\r\n",g_dwMinImageStart, g_dwMinImageStart+dwFormatLength);
            if (!CreateBINFSPartition(g_dwMinImageStart, dwFormatLength))
            {
                KITLOutputDebugString("Flash Create Partition failed!\r\n");
                return;
            }
        }

        // Are there multiple BIN files in RAM (we may just be updating one in a multi-BIN solution)?
        if (g_BINRegionInfo.dwNumRegions)
        {
            for (nCount = 0 ; nCount < g_BINRegionInfo.dwNumRegions ; nCount++)
            {
                DWORD dwRegionStart  = g_BINRegionInfo.Region[nCount].dwRegionStart;
                DWORD dwRegionLength = g_BINRegionInfo.Region[nCount].dwRegionLength;
                dwFlashAddress = (dwRegionStart ) + FLASH_BIN_START;
                // Remember the kernel's storage address.
                if (dwRegionStart == dwImageStart)
                    dwImgStoreAddr = dwFlashAddress;

                if (!OEMWriteFlash2(dwFlashAddress, (UCHAR*)dwRegionStart, dwRegionLength)) {
                    KITLOutputDebugString("ERROR: failed to store RAM image in flash.  Continuing...\r\n");
                    break;
                }
            }
        }
        else if (!OEMWriteFlash2(FLASH_BIN_START, (UCHAR*)dwImageStart, dwImageLength))
        {
            KITLOutputDebugString("ERROR: failed to store RAM image in flash.  Continuing...\r\n");
        }
        pBootArgs->dwImgStoreAddr=dwImgStoreAddr;
        pBootArgs->dwImgLoadAddr=dwImageStart;
        pBootArgs->dwImgLength=dwImageLength;
        KITLOutputDebugString("StoredBootArgs in NAND. dwImgStoreAddr=%x, dwImageStart=%x, dwImageLength=%x, Continuing...\r\n",
                dwImgStoreAddr,dwImageStart,dwImageLength);
        if (!WriteNANDStoredBootArgs(pBootArgs)) {
            KITLOutputDebugString("ERROR: failed to store StoredBootArgs in NAND.  Continuing...\r\n");
        }
        // Create an extended partition to use rest of flash to mount a filesystem.        
        if (!CreateExtendedPartition()) {
            KITLOutputDebugString("ERROR: failed to create extended partition.  Continuing...\r\n");
        }        
    }
    else 
    if ((pBootArgs->ucLoaderFlags & LDRFL_FLASH_BACKUP)!=0 ) {
        KITLOutputDebugString("NAND_FLASH do image restore\r\n");
        if (ReadNANDStoredBootArgs(pBootArgs)) {
            KITLOutputDebugString("Loading (Flash=0x%x  RAM=0x%x  Length=0x%x) (please wait).", pBootArgs->dwImgStoreAddr, pBootArgs->dwImgLoadAddr, pBootArgs->dwImgLength);
            if (!ReadNANDFlash(pBootArgs->dwImgStoreAddr, (LPBYTE)pBootArgs->dwImgLoadAddr, pBootArgs->dwImgLength))
            {
                KITLOutputDebugString("ERROR: Reading RAM image from flash failed.\r\n");
                SpinForever ();
            }
            dwLaunchAddr=pBootArgs->dwLaunchAddr;
        }
        else {
            KITLOutputDebugString("ERROR: Read StoredBootArgs from NAND.\r\n");
            SpinForever ();
        }
    }
#endif  // NAND_FLASH.
    KITLOutputDebugString("Lauch Windows CE from address 0x%x\r\n",dwLaunchAddr);
    ((PFN_LAUNCH)(dwLaunchAddr))();

    // never reached
    SpinForever ();
}
void
DrawProgress (int Percent, BOOL fBackground)
{
#if DRAW_PROGRESS_BAR
    int     i, j;
    int     iWidth = Percent*(pBootArgs->cyDisplayScreen/100);
    PWORD   pwData;
    PDWORD  pdwData;

    if ((pBootArgs->pvFlatFrameBuffer)) {
        // 20 rows high
        for (i=0; i < 20; i++) {
            switch (pBootArgs->bppScreen) {
            case 8 :
                memset ((PBYTE)pBootArgs->pvFlatFrameBuffer + pBootArgs->cbScanLineLength*(i+20), fBackground ? 3 : 15,
                        iWidth);
                break;
            case 16 :
                pwData = (PWORD)(pBootArgs->pvFlatFrameBuffer + pBootArgs->cbScanLineLength*(i+20));
                for (j=0; j < iWidth; j++) {
                    *pwData++ = fBackground ? 0x001f : 0xFFFF;
                }
                break;
            case 32 :
                pdwData = (PDWORD)(pBootArgs->pvFlatFrameBuffer + pBootArgs->cbScanLineLength*(i+20));
                for (j=0; j < iWidth; j++) {
                    *pdwData++ = fBackground ? 0x000000FF : 0xFFFFFFFF;
                }
                break;
            default :
                // Unsupported format, ignore
                break;
            }
        }
    }
#endif  // DRAW_PROGRESS_BAR.
}

// callback to receive data from transport
BOOL OEMReadData (DWORD cbData, LPBYTE pbData)
{
    BOOL fRetVal;
    int cPercent;
    if (fRetVal = EbootEtherReadData (cbData, pbData)) {
        g_dwOffset += cbData;
        if (g_dwLength) {
            cPercent = g_dwOffset*100/g_dwLength;
            DrawProgress (cPercent, FALSE);
        }
    }
    return fRetVal;
}

//
// Memory mapping related functions
//
LPBYTE OEMMapMemAddr (DWORD dwImageStart, DWORD dwAddr)
{
    // map address into physical address
    return (LPBYTE) (dwAddr & ~0x80000000);
}

//
// CEPC doesn't have FLASH, LED, stub the related functions
//
void OEMShowProgress (DWORD dwPacketNum)
{
}

BOOL OEMIsFlashAddr (DWORD dwAddr)
{
    return FALSE;
}

BOOL OEMStartEraseFlash (DWORD dwStartAddr, DWORD dwLength)
{
    return FALSE;
}

void OEMContinueEraseFlash (void)
{
}

BOOL OEMFinishEraseFlash (void)
{
    return FALSE;
}



//------------------------------------------------------------------------------
/* OEMEthGetFrame
 *
 *   Check to see if a frame has been received, and if so copy to buffer. An optimization
 *   which may be performed in the Ethernet driver is to filter out all received broadcast
 *   packets except for ARPs.  This is done in the SMC9000 driver.
 *
 * Return Value:
 *    Return TRUE if frame has been received, FALSE if not.
 */
//------------------------------------------------------------------------------
BOOL
OEMEthGetFrame(
    BYTE *pData,       // OUT - Receives frame data
    UINT16 *pwLength)  // IN  - Length of Rx buffer
                       // OUT - Number of bytes received
{
    return g_pEdbgDriver->pfnGetFrame (pData, pwLength);
}



//------------------------------------------------------------------------------
/* OEMEthSendFrame
 *
 *   Send Ethernet frame.
 *
 *  Return Value:
 *   TRUE if frame successfully sent, FALSE otherwise.
 */
//------------------------------------------------------------------------------
BOOL
OEMEthSendFrame(
    BYTE *pData,     // IN - Data buffer
    DWORD dwLength)  // IN - Length of buffer
{
    int retries = 0;

    // Let's be persistant here
    while (retries++ < 4) {
        if (!g_pEdbgDriver->pfnSendFrame (pData, dwLength))
            return TRUE;
        else
            KITLOutputDebugString("!OEMEthSendFrame failure, retry %u\n",retries);
    }
    return FALSE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID SC_WriteDebugLED(WORD wIndex, DWORD dwPattern) {
    return;
}

//
// rtc cmos functions, in oal_x86_rtc.lib
//
BOOL Bare_GetRealTime(LPSYSTEMTIME lpst);


//------------------------------------------------------------------------------
//  OEMGetRealTime
//
//  called by the kitl library to handle OEMEthGetSecs
//
//------------------------------------------------------------------------------
BOOL OEMGetRealTime(LPSYSTEMTIME lpst)
{
    return Bare_GetRealTime(lpst);
}

//
// read a line from debug port, return FALSE if timeout
// (timeout value in seconds)
//
#define BACKSPACE   8
#define IPADDR_MAX  15      // strlen ("xxx.xxx.xxx.xxx") = 15
static char szbuf[IPADDR_MAX+1];

static BOOL ReadIPLine (char *pbuf, DWORD dwTimeout)
{
    DWORD dwCurrSec = OEMEthGetSecs ();
    char ch;
    int nLen = 0;
    while (OEMEthGetSecs () - dwCurrSec < dwTimeout) {

        switch (ch = OEMReadDebugByte ()) {
        case OEM_DEBUG_COM_ERROR:
        case OEM_DEBUG_READ_NODATA:
            // no data or error, keep reading
            break;

        case BACKSPACE:
            nLen --;
            OEMWriteDebugByte (ch);
            break;

        case '\r':
        case '\n':
            OEMWriteDebugByte ('\n');
            pbuf[nLen] = 0;
            return TRUE;

        default:
            if ((ch == '.' || (ch >= '0' && ch <= '9')) && (nLen < IPADDR_MAX)) {
                pbuf[nLen ++] = ch;
                OEMWriteDebugByte (ch);
            }
        }
    }

    return FALSE;   // timeout
}

// ask user to select transport options
static BOOL AskUser (EDBG_ADDR *pMyAddr, DWORD *pdwSubnetMask)
{

    KITLOutputDebugString ("Hit ENTER within 3 seconds to enter static IP address!");
    szbuf[IPADDR_MAX] = 0;  // for safe measure, make sure null termination
    if (!ReadIPLine (szbuf, 3)) {
        return FALSE;
    }

    KITLOutputDebugString ("Enter IP address, or CR for default (%s): ", inet_ntoa (pMyAddr->dwIP));
    ReadIPLine (szbuf, INFINITE);
    if (szbuf[0]) {
        pMyAddr->dwIP = inet_addr (szbuf);
    }

    KITLOutputDebugString ("Enter Subnet Masks, or CR for default (%s): ", inet_ntoa (*pdwSubnetMask));
    ReadIPLine (szbuf, INFINITE);
    if (szbuf[0]) {
        *pdwSubnetMask = inet_addr (szbuf);
    }

    KITLOutputDebugString ( "\r\nUsing IP Address %s, ", inet_ntoa (pMyAddr->dwIP));
    KITLOutputDebugString ( "subnet mask %s\r\n", inet_ntoa (*pdwSubnetMask));
    return TRUE;
}
// Dummmy
LPVOID NKCreateStaticMapping(DWORD dwPhysBase, DWORD dwSize)
{
    return (LPVOID)dwPhysBase;
}


/*
    @func   void | OEMDownloadFileNotify | Receives/processed download file manifest.
    @rdesc  
    @comm    
    @xref   
*/
void OEMDownloadFileNotify(PDownloadManifest pInfo)
{
    BYTE nCount;

    if (!pInfo || !pInfo->dwNumRegions)
    {
        KITLOutputDebugString("WARNING: OEMDownloadFileNotify - invalid BIN region descriptor(s).\r\n");
        return;
    }

    g_dwMinImageStart = pInfo->Region[0].dwRegionStart;

    KITLOutputDebugString("\r\nDownload file information:\r\n");
    KITLOutputDebugString("-----------------------------------------------------\r\n");
    for (nCount = 0 ; nCount < pInfo->dwNumRegions ; nCount++)
    {
        EdbgOutputDebugString("[%d]: Address=0x%x  Length=0x%x  Name=%s\r\n", nCount, 
                                                                              pInfo->Region[nCount].dwRegionStart, 
                                                                              pInfo->Region[nCount].dwRegionLength, 
                                                                              pInfo->Region[nCount].szFileName);

        if (pInfo->Region[nCount].dwRegionStart < g_dwMinImageStart)
        {
            g_dwMinImageStart = pInfo->Region[nCount].dwRegionStart;
            if (g_dwMinImageStart == 0)
            {
                KITLOutputDebugString("WARNING: OEMDownloadFileNotify - bad start address for file (%s).\r\n", pInfo->Region[nCount].szFileName);
                return;
            }
        }
    }
    KITLOutputDebugString("\r\n");

    memcpy((LPBYTE)&g_BINRegionInfo, (LPBYTE)pInfo, sizeof(MultiBINInfo));
}


static PVOID GetKernelExtPointer(DWORD dwRegionStart, DWORD dwRegionLength)
{
    DWORD dwCacheAddress = 0;
    ROMHDR *pROMHeader;
    DWORD dwNumModules = 0;
    TOCentry *pTOC;

    if (dwRegionStart == 0 || dwRegionLength == 0)
        return(NULL);

    if (*(LPDWORD) OEMMapMemAddr (dwRegionStart, dwRegionStart + ROM_SIGNATURE_OFFSET) != ROM_SIGNATURE)
        return NULL;

    // A pointer to the ROMHDR structure lives just past the ROM_SIGNATURE (which is a longword value).  Note that
    // this pointer is remapped since it might be a flash address (image destined for flash), but is actually cached
    // in RAM.
    //
    dwCacheAddress = *(LPDWORD) OEMMapMemAddr (dwRegionStart, dwRegionStart + ROM_SIGNATURE_OFFSET + sizeof(ULONG));
    pROMHeader     = (ROMHDR *) OEMMapMemAddr (dwRegionStart, dwCacheAddress);

    // Make sure sure are some modules in the table of contents.
    //
    if ((dwNumModules = pROMHeader->nummods) == 0)
        return NULL;

    // Locate the table of contents and search for the kernel executable and the TOC immediately follows the ROMHDR.
    //
    pTOC = (TOCentry *)(pROMHeader + 1);

    while(dwNumModules--) {
        LPBYTE pFileName = OEMMapMemAddr(dwRegionStart, (DWORD)pTOC->lpszFileName);
        if (!strcmp((const char *)pFileName, "nk.exe")) {
            return ((PVOID)(pROMHeader->pExtensions));
        }
        ++pTOC;
    }
    return NULL;
}

ULONG 
HalSetBusDataByOffset(
    IN BUS_DATA_TYPE BusDataType,
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
{
    return (PCISetBusDataByOffset (BusNumber, SlotNumber, Buffer, Offset, Length));
}

BOOL INTERRUPTS_ENABLE (BOOL fEnable)
{
    return FALSE;
}

// For USB kitl it need this to satisfy the debug build.
#ifdef DEBUG
DBGPARAM
dpCurSettings =
{
    TEXT("RndisMini"),
    {
        //
        //  MDD's, DON"T TOUCH
        //

        TEXT("Init"),
        TEXT("Rndis"),
        TEXT("Host Miniport"),
        TEXT("CE Miniport"),
        TEXT("Mem"),
        TEXT("PDD Call"),
        TEXT("<unused>"),
        TEXT("<unused>"),

        //
        //  PDD can use these..
        //

        TEXT("PDD Interrupt"),
        TEXT("PDD EP0"),
        TEXT("PDD EP123"),
        TEXT("PDD FIFO"),       
        TEXT("PDD"),
        TEXT("<unused>"),


        //
        //  Standard, DON'T TOUCH
        //

        TEXT("Warning"),
        TEXT("Error")       
    },

    0xc000
};
#endif
