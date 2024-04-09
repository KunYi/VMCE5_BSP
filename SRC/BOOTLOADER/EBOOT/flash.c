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
#include <windows.h>
#ifdef NAND_FLASH
#include <bootarg.h>
#include <PCIReg.h>
#include <pci.h>
#include <ceddk.h>
#include <pehdr.h>
#include <bootpart.h>


#define FLASH_BASE 0x00000000
#define KSEG0_BASE 0xe0000000

#define NOT_FIXEDUP         (DWORD)-1
// Fix-up variable for total reserved area, which equals offset of the NK region in RAM
DWORD dwReservedArea = NOT_FIXEDUP;

// prototype
BOOL WriteAndVerifyFlash (DWORD dwStartAddr, LPBYTE pData, DWORD dwLength);
LPBYTE OEMMapMemAddr (DWORD dwImageStart, DWORD dwAddr);
HANDLE hBINFS =INVALID_HANDLE_VALUE;

BOOL ReadNANDStoredBootArgs(BOOT_ARGS *pBootArgs)
{
    EdbgOutputDebugString("ReadNANDStoredBootArgs\r\n");
    if ( pBootArgs->NANDBootFlags == NAND_PCI_PRESENT ) { // Susscess
        HANDLE hPartition = BP_OpenPartition(-1,1,PART_BOOTSECTION,FALSE,PART_OPEN_EXISTING);
        if (hPartition!=INVALID_HANDLE_VALUE) {
            DWORD dwSize =min(SECTOR_SIZE, sizeof(BOOT_ARGS));
            if (BP_SetDataPointer(hPartition,0) && BP_ReadData(hPartition, (PBYTE)pBootArgs, dwSize))
                return TRUE;
            EdbgOutputDebugString("ReadNANDStoredBootArgs : Can not Read Boot Partition Data \r\n");            
        }
        else 
            EdbgOutputDebugString("ReadNANDStoredBootArgs : Can not opened Boot Partition\r\n");
    }
    EdbgOutputDebugString("ReadNANDStoredBootArgs : Nand does not present\r\n");
    return FALSE;
}


BOOL WriteNANDStoredBootArgs(BOOT_ARGS * pBootArgs)
{
    EdbgOutputDebugString("WriteNANDStoredBootArgs\r\n");
    if ( pBootArgs->NANDBootFlags == NAND_PCI_PRESENT ) { // Susscess
        HANDLE hPartition = BP_OpenPartition(-1,1,PART_BOOTSECTION,FALSE,PART_OPEN_ALWAYS);
        if (hPartition!=INVALID_HANDLE_VALUE) {
            DWORD dwSize =min(SECTOR_SIZE, sizeof(BOOT_ARGS));
            if (BP_SetDataPointer (hPartition,0) && BP_WriteData(hPartition, (PBYTE)pBootArgs, dwSize))
                return TRUE;
            EdbgOutputDebugString("WriteNANDStoredBootArgs : Can not Write Boot Partition Data \r\n");            
        }
        else 
            EdbgOutputDebugString("WriteNANDStoredBootArgs : Can not opened Boot Partition\r\n");
    }
    EdbgOutputDebugString("WriteNANDStoredBootArgs : Nand does not present\r\n");
    return FALSE;
}
UCHAR uWorkingBuffer[0x9000];
BOOL FlashInit(BOOT_ARGS * pDriverGlobals)
{
    PCI_REG_INFO pciRegInfo;
    PCI_SLOT_NUMBER SlotNumber;
    EdbgOutputDebugString("FlashInit\r\n");
    // Initialize the FMD flash interface and get flash information.
    //
    if (!BP_Init(uWorkingBuffer, 0x9000,NULL,NULL,&pciRegInfo)) {
        pDriverGlobals->NANDBootFlags = 0;
        EdbgOutputDebugString("-FlashInit:Fails!!!!\r\n");
        return FALSE;
    }
    pDriverGlobals->NANDBootFlags    = NAND_PCI_PRESENT;
    pDriverGlobals->NANDBusNumber    =(UCHAR) pciRegInfo.Bus;
    SlotNumber.u.AsULONG = 0;
    SlotNumber.u.bits.DeviceNumber   = pciRegInfo.Device;
    SlotNumber.u.bits.FunctionNumber = pciRegInfo.Function;
    pDriverGlobals->NANDSlotNumber   = SlotNumber.u.AsULONG;
    hBINFS = INVALID_HANDLE_VALUE;

    if (dwReservedArea == NOT_FIXEDUP)
        dwReservedArea = 0;
    
    EdbgOutputDebugString("-FlashInit\r\n");
    return TRUE;
}


// These functions are implemented in order to make the NAND FMD happy.
VOID
SetLastError( DWORD dwErrCode )
{
}



VOID
MmUnmapIoSpace (
    IN PVOID BaseAddress,
    IN ULONG NumberOfBytes
    )
{
}


BOOL OEMWriteFlash2 (DWORD dwStartAddr, LPBYTE pData, DWORD dwLength)
{
	BOOL fRet;

    EdbgOutputDebugString("-------------------------------------------------------------------------------\r\n");

    fRet = WriteAndVerifyFlash (dwStartAddr, pData, dwLength);

    EdbgOutputDebugString("-------------------------------------------------------------------------------\r\n");
    return fRet;
}


BOOL OEMWriteFlash (DWORD dwStartAddr, DWORD dwLength)
{
	LPBYTE pData = (LPBYTE)OEMMapMemAddr(dwStartAddr, dwStartAddr);
    BOOL fRet = OEMWriteFlash2 (dwStartAddr, pData, dwLength);
    return fRet;
}



/*
    @func   BOOL | FormatFlash | Called when preparing flash for a multiple-BIN download.  
	                             Erases, verifies, and writes logical sector numbers in the range to be written.
    @rdesc  TRUE returned on success and FALSE on failure.
    @comm    
    @xref   
*/
BOOL CreateBINFSPartition(DWORD dwStart, DWORD dwLength)
{
    dwStart = (dwStart & ~KSEG0_BASE) - FLASH_BASE - dwReservedArea;
    EdbgOutputDebugString(" CreateBINFSPartition: Start =%x length =%len\r\n",dwStart,dwLength);
    hBINFS = BP_OpenPartition( -1,(dwStart+dwLength+SECTOR_SIZE)/SECTOR_SIZE,PART_BINFS,TRUE,PART_OPEN_ALWAYS);
    if (hBINFS == INVALID_HANDLE_VALUE ) {
        EdbgOutputDebugString("CreateBINFSPartition: FIALS!!!!\r\n");
        return FALSE;
    }
    EdbgOutputDebugString("-CreateBINFSPartition: \r\n");    
    return TRUE;
}
BOOL ReadNANDFlash(DWORD dwStartAddr, LPBYTE pBuffer, DWORD dwLength)
{
    EdbgOutputDebugString(" ReadNANDFlash: dwStartAddr=%x,pBuffer=%x,dwLength=%x \r\n",dwStartAddr,pBuffer,dwLength);
	if (!pBuffer || dwLength == 0)
		return(FALSE);

	// Compute sector address and length.
    dwStartAddr = ((dwStartAddr & ~KSEG0_BASE) - FLASH_BASE - dwReservedArea) ;
    if (hBINFS == INVALID_HANDLE_VALUE) {
        hBINFS = BP_OpenPartition( -1,-1,PART_BINFS,TRUE,PART_OPEN_EXISTING);
    }
    if (hBINFS != INVALID_HANDLE_VALUE ) {
        return (BP_SetDataPointer(hBINFS,dwStartAddr) && BP_ReadData(hBINFS, pBuffer, dwLength));
    }
    EdbgOutputDebugString(" ReadNANDFlash: FAILS!!!!!\r\n");
	return(TRUE);
}


BOOL WriteAndVerifyFlash (DWORD dwStartAddr, LPBYTE pData, DWORD dwLength)
{
    dwStartAddr = ((dwStartAddr & ~KSEG0_BASE) - FLASH_BASE - dwReservedArea) ;
    if (hBINFS == INVALID_HANDLE_VALUE) {
        hBINFS = BP_OpenPartition( -1,-1,PART_BINFS,TRUE,PART_OPEN_EXISTING);
    }
    if (hBINFS != INVALID_HANDLE_VALUE) {
        return (BP_SetDataPointer(hBINFS,dwStartAddr) && BP_WriteData(hBINFS, pData, dwLength));
    }
    EdbgOutputDebugString(" ReadNANDFlash: FAILS!!!!!\r\n");
    return(FALSE);
}

BOOL CreateExtendedPartition()
{
    EdbgOutputDebugString("CreateExtendedPartition\r\n");
    return (BP_OpenPartition(NEXT_FREE_LOC, USE_REMAINING_SPACE, PART_EXTENDED, TRUE, PART_OPEN_ALWAYS) != INVALID_HANDLE_VALUE);
}


#else
BOOL OEMWriteFlash (DWORD dwStartAddr, DWORD dwLength)
{
	return FALSE;
}
#endif



