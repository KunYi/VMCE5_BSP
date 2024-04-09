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

Abstract:  

Functions:


Notes: 

--*/

#include <windows.h>
#include <nkintr.h>
#include <pc.h>
#include <wdm.h>

#define LS_TSR_EMPTY        0x40
#define LS_THR_EMPTY        0x20
#define LS_RX_BREAK         0x10
#define LS_RX_FRAMING_ERR   0x08
#define LS_RX_PARITY_ERR    0x04
#define LS_RX_OVERRUN       0x02
#define LS_RX_DATA_READY    0x01

#define LS_RX_ERRORS        ( LS_RX_FRAMING_ERR | LS_RX_PARITY_ERR | LS_RX_OVERRUN )

#define IoPortBase          ( (PUCHAR) COM1_BASE )


void OEMInitDebugSerial(void)
{
    WRITE_PORT_UCHAR(IoPortBase+comLineControl, 0x80);   // Access Baud Divisor
    WRITE_PORT_UCHAR(IoPortBase+comDivisorLow, 0x03);    // 38400
    WRITE_PORT_UCHAR(IoPortBase+comDivisorHigh, 0x00);
    WRITE_PORT_UCHAR(IoPortBase+comFIFOControl, 0x01);   // Enable FIFO if present
    WRITE_PORT_UCHAR(IoPortBase+comLineControl, 0x03);   // 8 bit, no parity

    WRITE_PORT_UCHAR(IoPortBase+comIntEnable, 0x00);     // No interrupts, polled

    WRITE_PORT_UCHAR(IoPortBase+comModemControl, 0x03);  // Assert DTR, RTS
}

void OEMWriteDebugString(unsigned short *str)
{
    while ( *str ) {
        while ( !(READ_PORT_UCHAR(IoPortBase+comLineStatus) & LS_THR_EMPTY) ) {
            ;
        }

        WRITE_PORT_UCHAR(IoPortBase+comTxBuffer, (UCHAR)*str++);
    }
}

void OEMWriteDebugByte(BYTE ucChar)
{
    while ( !(READ_PORT_UCHAR(IoPortBase+comLineStatus) & LS_THR_EMPTY) ) {
        ;
    }

    WRITE_PORT_UCHAR(IoPortBase+comTxBuffer, ucChar);
}


void OEMWriteDWORD(DWORD dwVal)
{
    int i;
    const char conv[] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

    OEMWriteDebugByte('0');
    OEMWriteDebugByte('x');

    for (i=7;i>=0;i--) {
        char c;
        c = (char)((dwVal >> (i*4)) & 0xf);
        c = conv[c];
        OEMWriteDebugByte(c);
    }
    OEMWriteDebugByte(0x0d);
    OEMWriteDebugByte(0x0a);
}


int OEMReadDebugByte(void)
{
    unsigned char   ucStatus;
    unsigned char   ucChar;

    ucStatus = READ_PORT_UCHAR(IoPortBase+comLineStatus);

    if ( ucStatus & LS_RX_DATA_READY ) {
        ucChar = READ_PORT_UCHAR(IoPortBase+comRxBuffer);

        if ( ucStatus & LS_RX_ERRORS ) {
            return (OEM_DEBUG_COM_ERROR);
        } else {
            return (ucChar);
        }

    }

    return (OEM_DEBUG_READ_NODATA);
}


/*****************************************************************************
*
*
*   @func   void    |   OEMClearDebugComError | Clear a debug communications er
or
*
*/
void
OEMClearDebugCommError(
                      void
                      )
{
}
