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
#include <oalintr.h>
#include <ceddk.h>
#include "pc.h"
#include "timer.h"
#include <oemwake.h>
#include <oal.h>

#define Naked void __declspec(naked)
extern int (*PProfileInterrupt)(void);

extern volatile LARGE_INTEGER   CurTicks;

extern void KernelInitialize();
UCHAR PICGetCurrentInterrupt();

extern DWORD g_dwOALTimerCount;

//
// Warm reset handling
// 
DWORD dwRebootAddress;
void RebootHandler();


void OEMNMIHandler(void) {
}


// local version of PerfCountSinceTick that is callable from within PeRPISR()
DWORD   _PerfCountSinceTick(void)
{
    WORD wCurCount;

    __asm {
TRY_LATCHING_AGAIN: 
             
        pushfd                  ; Save interrupt state
        cli

        in      al,40h          ; clear latches, in case they are loaded
        in      al,40h          ; 
        mov     al,11000010b    ;latch status and count of counter 0
        out     43h,al
        in      al,40h          ; read status
        shl     eax, 16         ; move into high side of eax
        in      al,40h          ; read counter 0 lsb
        mov     ah,al
        in      al,40h          ; read msb
        xchg    ah, al          ; Get them in the right order
        mov     wCurCount, ax
        shr     eax, 16         ; get the status back into al
        popfd                   ; Restore interrupt state
        
        test    al, 40h         ; did the latch operation fail?
        jne     TRY_LATCHING_AGAIN     ; if so, just do it again
    }
    
    //
    // Note : this is a countdown timer, not count up.
    //

    return g_dwOALTimerCount - wCurCount;
}

// PeRP interrupt service routine.
//
//      This routine decode the various status registers on the PeRP to
// determine the cause of an interrupt and dispatches to the appropriate
// handler for that interrupt.
//
// Registers: v0, AT, a0-a3 available for use.
//
//      Entry   (a0) = 8 (hw interrupt 2)
//              (a1) = ptr to apvIntrData array
//      Exit    (v0) = interrupt dispostion information
//                      (see nkintr.h for values)


BOOL  fIntrTime;
DWORD dwIntrTimeIsr1;
DWORD dwIntrTimeIsr2;
DWORD dwIntrTimeNumInts;
DWORD dwIntrTimeCountdown;
DWORD dwIntrTimeCountdownRef;
DWORD dwIntrTimeSPC;

extern const DWORD g_dwBSPMsPerIntr;           // interrupt every ms

ULONG PeRPISR(void) {
    ULONG ulRet = SYSINTR_NOP;
    UCHAR ucCurrentInterrupt;
   
    if (fIntrTime) {
        //
        // We're doing interrupt timing. Get Time to ISR.
        //
#ifdef EXTERNAL_VERIFY
        _outp((USHORT)0x80, 0xE1);
#endif
        dwIntrTimeIsr1 = _PerfCountSinceTick();
        dwIntrTimeNumInts++;
    }
    
    ucCurrentInterrupt = PICGetCurrentInterrupt();

    if (ucCurrentInterrupt == INTR_TIMER0) {
        
        if (PProfileInterrupt) {
            ulRet= PProfileInterrupt();
        }

        if (!PProfileInterrupt || ulRet == SYSINTR_RESCHED) {
            
#ifdef SYSTIMERLED
            static BYTE bTick;
            _outp((USHORT)0x80, bTick++);
#endif
            
            CurMSec += g_dwBSPMsPerIntr;

            CurTicks.QuadPart += g_dwOALTimerCount;

            if (fIntrTime) {
                //
                // We're doing interrupt timing. Every nth tick is a SYSINTR_TIMING.
                //
                dwIntrTimeCountdown--;

                if (dwIntrTimeCountdown == 0) {
                    dwIntrTimeCountdown = dwIntrTimeCountdownRef;
                    dwIntrTimeNumInts = 0;
#ifdef EXTERNAL_VERIFY
                    _outp((USHORT)0x80, 0xE2);
#endif
                    dwIntrTimeIsr2 = _PerfCountSinceTick();
                    ulRet = SYSINTR_TIMING;
                } else {
                    if ((int) (CurMSec - dwReschedTime) >= 0)
                        ulRet = SYSINTR_RESCHED;
                }
            } else {
                if ((int) (CurMSec - dwReschedTime) >= 0)
                    ulRet = SYSINTR_RESCHED;
            }
        }
        
        //
        // Check if a reboot was requested.
        //
        if (dwRebootAddress) {
            RebootHandler();
        }
    
    } else if (ucCurrentInterrupt == INTR_RTC) {
        UCHAR cStatusC;
        // Check to see if this was an alarm interrupt
        cStatusC = CMOS_Read( RTC_STATUS_C);
        if((cStatusC & (RTC_SRC_IRQ|RTC_SRC_US)) == (RTC_SRC_IRQ|RTC_SRC_US))
            ulRet = SYSINTR_RTC_ALARM;
    } else if (ucCurrentInterrupt <= INTR_MAXIMUM) {  
        // We have a physical interrupt ID, but want to return a SYSINTR_ID

        // Call interrupt chain to see if any installed ISRs handle this interrupt
        ulRet = NKCallIntChain(ucCurrentInterrupt);

        // IRQ not claimed by installed ISR; translate into SYSINTR
        if (ulRet == SYSINTR_CHAIN) {
            ulRet = OALIntrTranslateIrq (ucCurrentInterrupt);
        }
		
//	    RETAILMSG(1, (TEXT("PeRPISR: intr=%d, sys_intr=%d\r\n"), ucCurrentInterrupt, ulRet));

        if (ulRet == SYSINTR_NOP || ulRet == SYSINTR_UNDEFINED || !NKIsSysIntrValid(ulRet)) {
            // If SYSINTR_NOP, IRQ claimed by installed ISR, but no further action required
            // If SYSINTR_UNDEFINED, ignore
            // If SysIntr was never initialized, ignore
            ulRet = SYSINTR_NOP;
        } else {
            // Valid SYSINTR, mask off interrupt source
            PICEnableInterrupt(ucCurrentInterrupt, FALSE);
        }
    }
    OEMIndicateIntSource(ulRet);
    if (ucCurrentInterrupt > 7 || ucCurrentInterrupt == -2) {
        __asm {
            mov al, 020h    ; Nonspecific EOI
            out 0A0h, al
        }
    }
    __asm {
        mov al, 020h        ; Nonspecific EOI
        out 020h, al
    }

    return ulRet;
}

void x86InitPICs(void) {
    int     i;
    __asm {
        ;
        ; First do PIC 1
        ;
        mov     al, 011h        ; Init command 1, cascade & 4th init byte
        out     020h, al
        jmp     short $+2

        mov     al, 040h        ; Init command 2, vector interrupts to 64
        out     021h, al
        jmp     short $+2

        mov     al, 004h        ; Init command 3, slave on IRQ 2
        out     021h, al
        jmp     short $+2

        mov     al, 001h        ; Init command 4, normal EOI
        out     021h, al
        jmp     short $+2

        mov     al, 00Bh        ; Select In Service Register for reads
        out     020h, al

        mov     al, 0FFh        ; Start with all interrupts disabled
        out     021h, al

        ;
        ; Now do PIC 2
        ;
        mov     al, 011h        ; Init command 1, cascade & 4th init byte
        out     0A0h, al
        jmp     short $+2

        mov     al, 048h        ; Init command 2, vector interrupts to 40
        out     0A1h, al
        jmp     short $+2

        mov     al, 002h        ; Init command 3, slave on IRQ 2
        out     0A1h, al
        jmp     short $+2

        mov     al, 001h        ; Init command 4, normal EOI
        out     0A1h, al

        mov     al, 00Bh        ; Select In Service Register for reads
        out     0A0h, al

        mov     al, 0FFh        ; Start with all interrupts disabled
        out     0A1h, al


        ; Clear any interrupts that might be pending (after a warm reboot).
        ;
        mov al, 020h		; PIC1
        out 020h, al

        mov al, 020h		; PIC2
        out 0A0h, al
    }
    /* Setup the PeRP interrupt handler and enable the PeRP interrupt in the BasePSR */
    for (i = 64; i < 80; i++)
        HookInterrupt(i, (void *)PeRPISR);
    //
    // Enable interrupts from cascaded PIC
    //
    PICEnableInterrupt(INTR_PIC2, TRUE);
}

VOID PICEnableInterrupt(UCHAR ucInterrupt, BOOL bEnabled) {
    __asm {
        pushfd                      ; Save interrupt state
        cli
        mov     dx, 021h            ; Assume its on PIC 1
        mov     cl, ucInterrupt
        mov     ch, 8
        cmp     cl, ch
        jl      lbl10
        mov     dx, 0A1h            ; Its actually on PIC 2
        sub     cl, ch
lbl10:
        mov     ch, 1               ; Convert interrupt number to bitmask
        shl     ch, cl

        in      al, dx              ; Get currently enabled interrupts

        test    bEnabled, 0FFh      ; Disable?
        jz      lbl20               ; Yes
        not     ch
        and     al, ch              ; Unmask interrupt
        jmp     short lbl30
lbl20:
        or      al, ch              ; Mask interrupt

lbl30:
        out     dx, al

        popfd                       ; Restore interrupt state
    }
}

UCHAR PICGetCurrentInterrupt() {
    UCHAR   ucActiveInterrupts;
    UCHAR   ucCurrentInterrupt;
    __asm {
        in  al, 020h    ; Get current interrupt
        mov ucActiveInterrupts, al
    }
    for (ucCurrentInterrupt = 0; ucCurrentInterrupt < 8; ucCurrentInterrupt++)
        if (ucActiveInterrupts & (1 << ucCurrentInterrupt)) 
            break;
    if (ucCurrentInterrupt == 8)
        return (UCHAR)-1;           // No interrupts pending on 1st PIC
    if (ucCurrentInterrupt == INTR_PIC2) {
        __asm {
            in  al, 0A0h    ; Get current interrupt
            mov ucActiveInterrupts, al
        }
        for (ucCurrentInterrupt = 0; ucCurrentInterrupt < 8; ucCurrentInterrupt++) 
            if (ucActiveInterrupts & (1 << ucCurrentInterrupt))
                break;
        if (ucCurrentInterrupt == 8)
            return (UCHAR)-2;       // No interrupts pending on 2nd PIC
        ucCurrentInterrupt += 8;
    }
    return ucCurrentInterrupt;
}


BOOL OALIntrEnableIrqs(UINT32 count, const UINT32 *pIrqs)
{
    UINT32 i;

    for (i = 0; i < count; i++) {
        PICEnableInterrupt ((UCHAR)pIrqs[i], TRUE);
    }        
    return TRUE;
}

VOID OALIntrDisableIrqs(UINT32 count, const UINT32 *pIrqs)
{
    UINT32 i;

    for (i = 0; i < count; i++) {
        PICEnableInterrupt((UCHAR)pIrqs[i], FALSE);
    }        
}

VOID OALIntrDoneIrqs(UINT32 count, const UINT32 *pIrqs)
{
    UINT32 i;

    for (i = 0; i < count; i++) {
        PICEnableInterrupt((UCHAR)pIrqs[i], TRUE);
    }        
}


