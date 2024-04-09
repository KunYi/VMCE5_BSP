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
    Serial boot loader main module. This file contains the C main
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
#include <ethdbg.h>
#include <nkintr.h>
#include <bootarg.h>

#include <pc.h>
#include <wdm.h>
#include <ceddk.h>
#include <pehdr.h>
#include <romldr.h>
#include "blcommon.h"

#define DEF_BAUD_RATE 115200
// DEF_BAUD_RATE must be one of 115200, 57600, 38400, 19200, 9600

// Use the serial bootloader in conjunction with the serial kitl transport
// Both the transport and the download service expect packets with the
// same header format. Packets not of type DLREQ, DLPKT, or DLACK are 
// ignored by the serial bootloader.

#define KITL_MAX_DEV_NAMELEN      16
#define KITL_MTU                  1520

// serial packet header and trailer definitions
static const UCHAR packetHeaderSig[] = { 'k', 'I', 'T', 'L' };
#define HEADER_SIG_BYTES    sizeof(packetHeaderSig)

#define KS_PKT_KITL         0xAA
#define KS_PKT_DLREQ        0xBB
#define KS_PKT_DLPKT        0xCC
#define KS_PKT_DLACK        0xDD
#define KS_PKT_JUMP         0xEE

// packet header
#include <pshpack1.h>
typedef struct tagSERIAL_PACKET_HEADER 
{
    UCHAR headerSig[HEADER_SIG_BYTES];
    UCHAR pktType;
    UCHAR Reserved;
    USHORT payloadSize; // not including this header    
    UCHAR crcData;
    UCHAR crcHdr;

} SERIAL_PACKET_HEADER, *PSERIAL_PACKET_HEADER;
#include <poppack.h>

// packet payload
typedef struct tagSERIAL_BOOT_REQUEST 
{
    UCHAR PlatformId[KITL_MAX_DEV_NAMELEN+1];
    UCHAR DeviceName[KITL_MAX_DEV_NAMELEN+1];
    USHORT reserved;

} SERIAL_BOOT_REQUEST, *PSERIAL_BOOT_REQUEST;

typedef struct tagSERIAL_BOOT_ACK
{
    DWORD fJumping;

} SERIAL_BOOT_ACK, *PSERIAL_BOOT_ACK;

typedef struct tagSERIAL_JUMP_REQUST
{
    DWORD dwKitlTransport;
    
} SERIAL_JUMP_REQUEST, *PSERIAL_JUMP_REQUEST;

typedef struct tagSERIAL_BLOCK_HEADER
{
    DWORD uBlockNum;
    
} SERIAL_BLOCK_HEADER, *PSERIAL_BLOCK_HEADER;

#define BOOT_ARG_PTR_LOCATION_NP    0x001FFFFC
#define BOOT_ARG_LOCATION_NP        0x001FFF00

#define PLATFORM_STRING "CEPC"

// serial port definitions
#define COM_RTS             0x02
#define COM_DTR             0x01

// line status register
#define LS_TSR_EMPTY        0x40
#define LS_THR_EMPTY        0x20
#define LS_RX_BREAK         0x10
#define LS_RX_FRAMING_ERR   0x08
#define LS_RX_PARITY_ERR    0x04
#define LS_RX_OVERRUN       0x02
#define LS_RX_DATA_READY    0x01
#define LS_RX_ERRORS        ( LS_RX_FRAMING_ERR | LS_RX_PARITY_ERR | LS_RX_OVERRUN )

// modem status register
#define MS_CARRIER_DETECT   0x80
#define MS_RING             0x40
#define MS_DSR              0x20
#define MS_CTS              0x10
#define MS_DELTA_DCD        0x08
#define MS_TRAIL_EDGE_RI    0x04
#define MS_DELTA_DSR        0x02
#define MS_DELTA_CTS        0x01

#define DEF_BAUD_DIVISOR    (115200/DEF_BAUD_RATE)

static BOOT_ARGS *pBootArgs;
__declspec(align(4)) BYTE g_buffer[KITL_MTU];
static PUCHAR DlIoPortBase = 0;

// OS launch function type
typedef void (*PFN_LAUNCH)();

// prototypes
BOOL OEMSerialSendRaw(LPBYTE pbFrame, USHORT cbFrame);
BOOL OEMSerialRecvRaw(LPBYTE pbFrame, PUSHORT pcbFrame, BOOLEAN bWaitInfinite);
BOOL RecvHeader(PSERIAL_PACKET_HEADER pHeader, BOOLEAN bWaitInfinite);
BOOL RecvPacket(PSERIAL_PACKET_HEADER pHeader, PBYTE pbFrame, PUSHORT pcbFrame, BOOLEAN bWaitInfinite);
BOOL WaitForJump(VOID);
BOOL WaitForBootAck(BOOL *pfJump);
BOOL SerialSendBlockAck(DWORD uBlockNumber);
BOOL SerialSendBootRequest(VOID);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
SpinForever(void)
{
    EdbgOutputDebugString("SpinForever...\r\n");
    while(1);
}

BOOL OEMDebugInit (void)
{
    //
    // Initialize the monitor port (for development system)
    // This should be done first since otherwise we can not
    // print any message onto the monitor
    //
    OEMInitDebugSerial();
    return TRUE;
}

//------------------------------------------------------------------------------
BOOL OEMPlatformInit (void)
//------------------------------------------------------------------------------
{    
    extern void BootUp (void);
    
    EdbgOutputDebugString("Microsoft Windows CE Serial Bootloader for CE/PC (%s)\n", __DATE__);
    
    //
    // Get pointer to Boot Args...
    //
    pBootArgs = (BOOT_ARGS *) ((ULONG)(*(PBYTE *)BOOT_ARG_PTR_LOCATION_NP));

    pBootArgs->dwEBootAddr = (DWORD)BootloaderMain;
    pBootArgs->ucLoaderFlags = 0;   

    // set up comm port
    DlIoPortBase = (PUCHAR)COM2_BASE;
    WRITE_PORT_UCHAR(DlIoPortBase+comIntEnable,     0x00);  // Interrupts off
    WRITE_PORT_UCHAR(DlIoPortBase+comLineControl,   0x80);  // Access Baud Divisor (DLB)
    WRITE_PORT_UCHAR(DlIoPortBase+comDivisorLow,    DEF_BAUD_DIVISOR);
    WRITE_PORT_UCHAR(DlIoPortBase+comDivisorHigh,   0x00);  // 
    WRITE_PORT_UCHAR(DlIoPortBase+comFIFOControl,   0x07);  // FIFO on, reset
    WRITE_PORT_UCHAR(DlIoPortBase+comLineControl,   0x03);  // 8 Data Bits, 1 Stop Bit, No Parity
    WRITE_PORT_UCHAR(DlIoPortBase+comModemControl,  0x00);  // de-assert DTR, de-assert RTS

    // clear comm errors
    READ_PORT_UCHAR(DlIoPortBase+comLineStatus);

    EdbgOutputDebugString("Comm Port initialized @ 0x%x\n", DlIoPortBase);
    
    return TRUE;
}

//------------------------------------------------------------------------------
DWORD OEMPreDownload (void)
//------------------------------------------------------------------------------
{   
    BOOL fGotJump = FALSE;

    // send boot requests indefinitely
    do
    {
        EdbgOutputDebugString("Sending boot request...\r\n");
        if(!SerialSendBootRequest())
        {
            EdbgOutputDebugString("Failed to send boot request\r\n");
            return BL_ERROR;
        }
    }
    while(!WaitForBootAck(&fGotJump));

    // ack block zero to start the download
    SerialSendBlockAck(0);

    EdbgOutputDebugString("Received boot request ack... starting download\r\n");

    return fGotJump  ? BL_JUMP : BL_DOWNLOAD;
}

//------------------------------------------------------------------------------
void OEMLaunch (DWORD dwImageStart, DWORD dwImageLength, DWORD dwLaunchAddr, const ROMHDR *pRomHdr)
//------------------------------------------------------------------------------
{    
    EdbgOutputDebugString ("Download successful! Jumping to image at %Xh...\r\n", dwLaunchAddr);

    // if no launch address specified, get it from boot args
    if(!dwLaunchAddr) 
    {
        dwLaunchAddr = pBootArgs->dwLaunchAddr;
    }

    // save launch address in the boot args
    else
    {
        pBootArgs->dwLaunchAddr = dwLaunchAddr;
    }

    // wait for jump packet indefinitely
    if(WaitForJump())
    {
        ((PFN_LAUNCH)(dwLaunchAddr))();
    }

    EdbgOutputDebugString("Failed to wait for jump message\r\n");
    
    SpinForever ();
}

//------------------------------------------------------------------------------
BOOL OEMReadData (DWORD cbData, LPBYTE pbData)
//------------------------------------------------------------------------------
{
    static DWORD dwBlockNumber = 0;
    
    static USHORT cbDataBuffer = 0;    
    static BYTE dataBuffer[KITL_MTU] = {0};

    // the first DWORD in the local buffer is the block header which contains
    // the sequence number of the block received
    static PSERIAL_BLOCK_HEADER pBlockHeader = (PSERIAL_BLOCK_HEADER)dataBuffer;
    static PBYTE pBlock = dataBuffer + sizeof(PSERIAL_BLOCK_HEADER);

    SERIAL_PACKET_HEADER header;
    USHORT cbPacket;

    LPBYTE pOutBuffer = pbData;

    while(cbData)
    {
        // if there is no data in the local buffer, read some
        //
        while(0 == cbDataBuffer)
        {
             // read from port
             cbPacket = sizeof(dataBuffer);
             if(RecvPacket(&header, dataBuffer, &cbPacket, TRUE))
             {
                // ignore non-download packet types
                if(KS_PKT_DLPKT == header.pktType)
                {                    
                    // make sure we received the correct block in the sequence
                    if(dwBlockNumber == pBlockHeader->uBlockNum)
                    {
                        cbDataBuffer = header.payloadSize - sizeof(SERIAL_BLOCK_HEADER);
                        dwBlockNumber++;
                    }
                    else
                    {
                        EdbgOutputDebugString("Received out of sequence block %u\r\n", pBlockHeader->uBlockNum);
                        EdbgOutputDebugString("Expected block %u\r\n", dwBlockNumber);
                    }
                    
                    // ack, or re-ack the sender
                    if(dwBlockNumber > 0)
                    {
                        // block number has already been incremented when appropriate
                        SerialSendBlockAck(dwBlockNumber - 1);
                    }
                }
             }
        }
        
        // copy from local buffer into output buffer
        //

        // if there are more than the requested bytes, copy and shift
        // the local data buffer
        if(cbDataBuffer > cbData)
        {
            // copy requested bytes from local buffer into output buffer            
            memcpy(pOutBuffer, pBlock, cbData);
            cbDataBuffer -= (USHORT)cbData;

            // shift the local buffer accordingly because not all data was used
            memmove(pBlock, pBlock + cbData, cbDataBuffer);
            cbData = 0;
        }
        else // cbDataBuffer <= cbData
        {
            // copy all bytes in local buffer to output buffer
            memcpy(pOutBuffer, pBlock, cbDataBuffer);
            cbData -= cbDataBuffer;
            pOutBuffer += cbDataBuffer;
            cbDataBuffer = 0;
        }
    }

    return TRUE;
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

BOOL OEMWriteFlash (DWORD dwStartAddr, DWORD dwLength)
{
    return FALSE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

VOID SC_WriteDebugLED(WORD wIndex, DWORD dwPattern) {
    return;
}

//------------------------------------------------------------------------------
BYTE CMOS_Read( BYTE offset )
{
    BYTE cAddr, cResult;
    
    // Remember, we only change the low order 5 bits in address register
    cAddr = _inp( CMOS_ADDR );
    _outp( CMOS_ADDR, (cAddr & RTC_ADDR_MASK) | offset );    
    cResult = _inp( CMOS_DATA );
    
    return (cResult);
}

BOOL IsTimeEqual(LPSYSTEMTIME lpst1, LPSYSTEMTIME lpst2) 
{
    if (lpst1->wYear != lpst2->wYear)           return(FALSE);
    if (lpst1->wMonth != lpst2->wMonth)         return(FALSE);
    if (lpst1->wDayOfWeek != lpst2->wDayOfWeek) return(FALSE);
    if (lpst1->wDay != lpst2->wDay)             return(FALSE);
    if (lpst1->wHour != lpst2->wHour)           return(FALSE);
    if (lpst1->wMinute != lpst2->wMinute)       return(FALSE);
    if (lpst1->wSecond != lpst2->wSecond)       return(FALSE);

    return (TRUE);
}

// RTC routines from kernel\hal\x86\rtc.c
BOOL
Bare_GetRealTime(LPSYSTEMTIME lpst)
{
    SYSTEMTIME st;
    LPSYSTEMTIME lpst1 = &st, lpst2 = lpst, lptmp;

    lpst1->wSecond = 61;    // initialize to an invalid value 
    lpst2->wSecond = 62;    // initialize to an invalid value 

    do {
   
        // exchange lpst1 and lpst2
       lptmp = lpst1;
       lpst1 = lpst2;
       lpst2 = lptmp;
   
        // wait until not updating
       while (CMOS_Read(RTC_STATUS_A) & RTC_SRA_UIP);
   
       // Read all the values.
       lpst1->wYear = CMOS_Read(RTC_YEAR);
       lpst1->wMonth = CMOS_Read(RTC_MONTH); 
       lpst1->wDayOfWeek = CMOS_Read(RTC_DO_WEEK);
       lpst1->wDay = CMOS_Read(RTC_DO_MONTH);
       lpst1->wHour = CMOS_Read(RTC_HOUR); 
       lpst1->wMinute = CMOS_Read(RTC_MINUTE); 
       lpst1->wSecond = CMOS_Read(RTC_SECOND); 
   
    } while (!IsTimeEqual (lpst1, lpst2));
   
    lpst->wMilliseconds = 0; // No easy way to read Milliseconds, returning 0 is safe
   
    if (!(CMOS_Read (RTC_STATUS_B) & RTC_SRB_DM)) {
        // Values returned in BCD.
       lpst->wSecond = DECODE_BCD(lpst->wSecond);
       lpst->wMinute = DECODE_BCD(lpst->wMinute);
       lpst->wHour   = DECODE_BCD(lpst->wHour);
       lpst->wDay    = DECODE_BCD(lpst->wDay);
       lpst->wDayOfWeek = DECODE_BCD(lpst->wDayOfWeek);
       lpst->wMonth  = DECODE_BCD(lpst->wMonth);
       lpst->wYear   = DECODE_BCD(lpst->wYear);
    }
   
    // OK - PC RTC returns 1998 as 98.
    lpst->wYear += (lpst->wYear > 70)? 1900 : 2000;
   
    return (TRUE);
}

//------------------------------------------------------------------------------
//  OEMGetSecs
//
//  Return a count of seconds from some arbitrary time (the absolute value 
//  is not important, so long as it increments appropriately).
//
//------------------------------------------------------------------------------
DWORD
OEMGetSecs()
{
    SYSTEMTIME st;
    Bare_GetRealTime( &st );
    return ((60UL * (60UL * (24UL * (31UL * st.wMonth + st.wDay) + st.wHour) + st.wMinute)) + st.wSecond);
}


//------------------------------------------------------------------------------
UCHAR CalcChksum(PUCHAR pBuf, USHORT len)
//------------------------------------------------------------------------------
{
    USHORT s = 0;
    UCHAR csum = 0;

    for(s = 0; s < len; s++)
        csum += *(pBuf + s);
    
    return csum;
}

//------------------------------------------------------------------------------
BOOL SerialSendBootRequest(VOID)
//------------------------------------------------------------------------------
{
    BYTE buffer[sizeof(SERIAL_PACKET_HEADER) + sizeof(SERIAL_BOOT_REQUEST)] = {0};
    PSERIAL_PACKET_HEADER pHeader = (PSERIAL_PACKET_HEADER)buffer;
    PSERIAL_BOOT_REQUEST pBootReq = (PSERIAL_BOOT_REQUEST)(buffer + sizeof(SERIAL_PACKET_HEADER));

    // create boot request
    strcpy(pBootReq->PlatformId, PLATFORM_STRING);

    // create header
    memcpy(pHeader->headerSig, packetHeaderSig, HEADER_SIG_BYTES);
    pHeader->pktType = KS_PKT_DLREQ;
    pHeader->payloadSize = sizeof(SERIAL_BOOT_REQUEST);
    pHeader->crcData = CalcChksum((PBYTE)pBootReq, sizeof(SERIAL_BOOT_REQUEST));
    pHeader->crcHdr = CalcChksum((PBYTE)pHeader, 
        sizeof(SERIAL_PACKET_HEADER) - sizeof(pHeader->crcHdr));

    OEMSerialSendRaw(buffer, sizeof(SERIAL_PACKET_HEADER) + sizeof(SERIAL_BOOT_REQUEST));

    return TRUE;
}

//------------------------------------------------------------------------------
BOOL SerialSendBlockAck(DWORD uBlockNumber)
//------------------------------------------------------------------------------
{
    BYTE buffer[sizeof(SERIAL_PACKET_HEADER) + sizeof(SERIAL_BLOCK_HEADER)];
    PSERIAL_PACKET_HEADER pHeader = (PSERIAL_PACKET_HEADER)buffer;
    PSERIAL_BLOCK_HEADER pBlockAck = (PSERIAL_BLOCK_HEADER)(buffer + sizeof(SERIAL_PACKET_HEADER));

    // create block ack
    pBlockAck->uBlockNum = uBlockNumber;

    // create header
    memcpy(pHeader->headerSig, packetHeaderSig, HEADER_SIG_BYTES);
    pHeader->pktType = KS_PKT_DLACK;
    pHeader->payloadSize = sizeof(SERIAL_BLOCK_HEADER);
    pHeader->crcData = CalcChksum((PBYTE)pBlockAck, sizeof(SERIAL_BLOCK_HEADER));
    pHeader->crcHdr = CalcChksum((PBYTE)pHeader,
        sizeof(SERIAL_PACKET_HEADER) - sizeof(pHeader->crcHdr));

    OEMSerialSendRaw(buffer, sizeof(SERIAL_PACKET_HEADER) + sizeof(SERIAL_BLOCK_HEADER));

    return TRUE;
}

//------------------------------------------------------------------------------
BOOL WaitForBootAck(BOOL *pfJump)
//
//  This function will fail if a boot ack is not received in a timely manner
//  so that another boot request can be sent
//------------------------------------------------------------------------------
{
    BOOL fRet = FALSE;
    USHORT cbBuffer = KITL_MTU;
    SERIAL_PACKET_HEADER header = {0};
    PSERIAL_BOOT_ACK pBootAck = (PSERIAL_BOOT_ACK)g_buffer;

    EdbgOutputDebugString("Waiting for boot ack...\r\n");
    if(RecvPacket(&header, g_buffer, &cbBuffer, FALSE))
    {
        // header checksum already verified
        if(KS_PKT_DLACK == header.pktType && 
            sizeof(SERIAL_BOOT_ACK) == header.payloadSize)
        {
            EdbgOutputDebugString("Received boot ack\r\n");
            *pfJump = pBootAck->fJumping;
            fRet = TRUE;
        }
    }

    return fRet;
}

//------------------------------------------------------------------------------
BOOL WaitForJump(VOID)
//  
//  waits indefinitely for jump command
//------------------------------------------------------------------------------
{
    USHORT cbBuffer = KITL_MTU;
    SERIAL_PACKET_HEADER header = {0};
    PSERIAL_JUMP_REQUEST pJumpReq = (PSERIAL_JUMP_REQUEST)g_buffer;

    // wait indefinitely for a jump request
    while(1)
    {
        if(RecvPacket(&header, g_buffer, &cbBuffer, TRUE))
        {
            // header & checksum already verified
            if(KS_PKT_JUMP == header.pktType && 
                sizeof(SERIAL_JUMP_REQUEST) == header.payloadSize)
            {
                pBootArgs->KitlTransport = (WORD)pJumpReq->dwKitlTransport;
                SerialSendBlockAck(0);
                return TRUE;
            }
        }
    }

    // never reached
    return FALSE;    
}

#define TIMEOUT_RECV    3 // seconds

//------------------------------------------------------------------------------
BOOL RecvPacket(PSERIAL_PACKET_HEADER pHeader, PBYTE pbFrame, PUSHORT pcbFrame, BOOLEAN bWaitInfinite)
//------------------------------------------------------------------------------
{
    // receive header
    if(!RecvHeader(pHeader, bWaitInfinite))
    {
        EdbgOutputDebugString("failed to receive header\r\n");
        return FALSE;
    }

    // verify packet checksum
    if(pHeader->crcHdr != CalcChksum((PBYTE)pHeader, 
        sizeof(SERIAL_PACKET_HEADER) - sizeof(pHeader->crcHdr)))
    {
        EdbgOutputDebugString("header checksum failure\r\n");
        return FALSE;
    }

    // make sure sufficient buffer is provided
    if(*pcbFrame < pHeader->payloadSize)
    {
        EdbgOutputDebugString("insufficient buffer size; ignoring packet\r\n");
        return FALSE;
    }

    // receive data
    *pcbFrame = pHeader->payloadSize;
    if(!OEMSerialRecvRaw(pbFrame, pcbFrame, bWaitInfinite))
    {
        EdbgOutputDebugString("failed to read packet data\r\n");
        return FALSE;
    }

    // verify data checksum
    if(pHeader->crcData != CalcChksum(pbFrame, *pcbFrame))
    {
        EdbgOutputDebugString("data checksum failure\r\n");
        return FALSE;
    }

    // verify packet type -- don't return any packet that is not
    // a type the bootloader expects to receive
    if(KS_PKT_DLPKT != pHeader->pktType &&
       KS_PKT_DLACK != pHeader->pktType &&
       KS_PKT_JUMP != pHeader->pktType)
    {
        EdbgOutputDebugString("received non-download packet type\r\n");
        return FALSE;
    }

    return TRUE;
}

//------------------------------------------------------------------------------
BOOL RecvHeader(PSERIAL_PACKET_HEADER pHeader, BOOLEAN bWaitInfinite)
//------------------------------------------------------------------------------
{
    USHORT cbRead;
    UINT i = 0;
    cbRead = sizeof(UCHAR);
    // read the header bytes
    while(i < HEADER_SIG_BYTES)
    {
        if(!OEMSerialRecvRaw((PBYTE)&(pHeader->headerSig[i]), &cbRead, bWaitInfinite) || sizeof(UCHAR) != cbRead)
        {
            EdbgOutputDebugString("failed to receive header signature\r\n");
            return FALSE;
        }

        if(pHeader->headerSig[i] == packetHeaderSig[i])
        {
            i++;
        }

        else
        {
            i = 0;
        }
    }

    // read the remaining header
    cbRead = sizeof(SERIAL_PACKET_HEADER) - HEADER_SIG_BYTES;
    if(!OEMSerialRecvRaw((PUCHAR)pHeader + HEADER_SIG_BYTES, &cbRead, bWaitInfinite) ||
        sizeof(SERIAL_PACKET_HEADER) - HEADER_SIG_BYTES != cbRead)
    {
        EdbgOutputDebugString("failed to receive header data\r\n");
        return FALSE;
    }
    
    // verify the header checksum
    if(pHeader->crcHdr != CalcChksum((PUCHAR)pHeader, 
        sizeof(SERIAL_PACKET_HEADER) - sizeof(pHeader->crcHdr)))
    {
        EdbgOutputDebugString("header checksum fail\r\n");
        return FALSE;
    }

    return TRUE;
}

//------------------------------------------------------------------------------
BOOL OEMSerialRecvRaw(LPBYTE pbFrame, PUSHORT pcbFrame, BOOLEAN bWaitInfinite)
//------------------------------------------------------------------------------
{
    USHORT ct = 0;
    DWORD tStart = 0;
    UCHAR uStatus = 0;

    UCHAR uCtrl = READ_PORT_UCHAR(DlIoPortBase+comModemControl) & ~COM_RTS;
    WRITE_PORT_UCHAR(DlIoPortBase+comModemControl, (UCHAR)(uCtrl | COM_RTS));

    for(ct = 0; ct < *pcbFrame; ct++)
    {
		if (!bWaitInfinite)
		{
			tStart = OEMGetSecs();
		}

		while(!(LS_RX_DATA_READY & (uStatus = READ_PORT_UCHAR(DlIoPortBase+comLineStatus))))
        {
            if(!bWaitInfinite && (OEMGetSecs() - tStart > TIMEOUT_RECV))
            {
                *pcbFrame = 0;
                WRITE_PORT_UCHAR(DlIoPortBase+comModemControl, (UCHAR)(uCtrl));
                return FALSE;
            }            
        }

        // check and clear comm errors
        if(LS_RX_ERRORS & uStatus)
        {
            // clear the queue
            WRITE_PORT_UCHAR(DlIoPortBase+comFIFOControl,   0x07);
            
            *pcbFrame = 0;
            WRITE_PORT_UCHAR(DlIoPortBase+comModemControl, (UCHAR)(uCtrl));

            EdbgOutputDebugString("Comm errors have occurred; status = 0x%x\r\n", uStatus);
            return FALSE;
        }

        *(pbFrame + ct) = READ_PORT_UCHAR(DlIoPortBase+comRxBuffer);
    }
    WRITE_PORT_UCHAR(DlIoPortBase+comModemControl, (UCHAR)(uCtrl));
    
    return TRUE;
}

//------------------------------------------------------------------------------
BOOL OEMSerialSendRaw(LPBYTE pbFrame, USHORT cbFrame)
//------------------------------------------------------------------------------
{
    UINT ct;
    // block until send is complete; no timeout
    for(ct = 0; ct < cbFrame; ct++)
    {            
        // check that send transmitter holding register is empty
        while(!(READ_PORT_UCHAR(DlIoPortBase+comLineStatus) & LS_THR_EMPTY))
            (VOID)NULL;

        // write character to port
        WRITE_PORT_UCHAR(DlIoPortBase+comTxBuffer, (UCHAR)*(pbFrame+ct));
    }
    
    return TRUE;
}

