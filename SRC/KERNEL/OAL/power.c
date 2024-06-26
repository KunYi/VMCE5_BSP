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
// Power off/Suspend routines
//

#include <windows.h>
#include <nkintr.h>
#include <oalintr.h>
#include "pc.h"
#include <oal.h>
#include <oal_nkxp.h>

#define NOTDEF

static UCHAR GetPICInterruptMask(BOOL fMaster);
static BOOL SetPICInterruptMask(BOOL fMaster, UCHAR ucMask);

//
// This routine is invoked when the OFF button is pressed. It is responsible
// for any final power off state and putting the cpu into standby.
//
static CpuSleepX86(void) {

#ifdef NOTDEF
    // NOTE : This bit of code will reboot the system. Some people build this way
    // so that they can do a soft reboot simply by choosing suspend from the run menu.
    __asm {
        cli                     ; Disable Interrupts
        mov     al, 0FEh        ; Reset CPU
        out     064h, al
        jmp     near $          ; Should not get here
    }
#endif

    __asm {
        sti
        hlt
        cli
    }
}


static UCHAR GetPICInterruptMask(BOOL fMaster) {
    UCHAR ucMask;
    UCHAR ucPort = (fMaster)?0x21:0xa1;

    __asm {
        pushfd
        cli
        xor dx, dx
        mov dl, ucPort
        in al, dx
        mov ucMask, al
        sti
        popfd
    }

    return ucMask;
}


static BOOL SetPICInterruptMask(BOOL fMaster, UCHAR ucMask) {
    WORD wPort = (fMaster)?0x21:0xa1;

    __asm {
        pushfd                      ; Save interrupt state
        cli
        mov     dx, wPort
        mov     al, ucMask
        out     dx, al
        sti
        popfd                       ; Restore interrupt state
    }

    return TRUE;
}

static DWORD dwLastWakeupSource=SYSWAKE_UNKNOWN;

static BYTE fInterruptWakeup[SYSINTR_MAXIMUM];
static BYTE fInterruptWakeupMask[SYSINTR_MAXIMUM];

static void SetInterruptMask(PBYTE pMaster,PBYTE pSlave)
{
    DWORD sysIntr;
    *pMaster=*pSlave=0;
    for (sysIntr = 0; sysIntr < SYSINTR_MAXIMUM; sysIntr++) {
        if (fInterruptWakeupMask[sysIntr]) {
             UINT32 count, *pIrqs, i;
             count = 1;
             if (!OALIntrTranslateSysIntr(sysIntr, &count, &pIrqs)) continue;
             for (i = 0; i < count; i++) {
                 if (pIrqs[i] != -1)  {
                    if (pIrqs[i] < 8) 
                        *pMaster|=(1 << pIrqs[i]);
                    else 
                        *pSlave |=(1 << (pIrqs[i] - 8));
                }
             }                 
        }
    }

}

DWORD OEMPowerManagerInit(void)
{
#ifdef INIT_WAKE
    // OEMPowerOff used to hardcode the keyboard and mouse to wake the system.
    // Device drivers can now tell the OAL to enable/disable it's wake interrupt.
    // Define INIT_WAKE if you want to use the hardcoded values.
    OEMSetWakeupSource(SYSINTR_KEYBOARD);
    OEMSetWakeupSource(SYSINTR_TOUCH);
#endif
    return 0;
}

DWORD OEMSetWakeupSource( DWORD dwSources)
{
    if (dwSources<SYSINTR_MAXIMUM) {
        fInterruptWakeupMask[dwSources]=1;
        return 1;
    } 
    return 0;
}

DWORD OEMResetWakeupSource( DWORD dwSources)
{
    if (dwSources<SYSINTR_MAXIMUM) {
        fInterruptWakeupMask[dwSources]=0;
        return 1;
    } 
    return 0;
}

DWORD OEMGetWakeupSource(void)
{
    return dwLastWakeupSource;
}

void OEMIndicateIntSource(DWORD dwSources)
{
    if (dwSources<SYSINTR_MAXIMUM ) {
        fInterruptWakeup[dwSources]=1;
    }
}

void OEMClearIntSources()
{
    memset(fInterruptWakeup,0,SYSINTR_MAXIMUM);
}

DWORD OEMFindFirstWakeSource()
{
    DWORD dwCount;

    for (dwCount=0;dwCount<SYSINTR_MAXIMUM;dwCount++) {
        if (fInterruptWakeup[dwCount]&& fInterruptWakeupMask[dwCount])
            break;
    }

    return (dwCount == SYSINTR_MAXIMUM) ? SYSWAKE_UNKNOWN : dwCount;
}

void OEMPowerOff(void)
{
    BYTE bOldMaster,bOldSlave;
    BYTE bMaster,bSlave;
    DWORD dwWake;
    BOOL fWakePending = FALSE;

    INTERRUPTS_OFF();

    // clear our wake source, which is the reason we are exiting this routine
    dwLastWakeupSource = SYSWAKE_UNKNOWN;
        
    // get the current interrupt mask
    bOldMaster=GetPICInterruptMask(TRUE);
    bOldSlave=GetPICInterruptMask(FALSE);

    // configure the new mask and clear wake source globals
    SetInterruptMask(&bMaster,&bSlave);
    SetPICInterruptMask( TRUE, (UCHAR)~(bMaster| (1<<INTR_PIC2)));
    SetPICInterruptMask( FALSE, (UCHAR)~bSlave );

    do {
       INTERRUPTS_ON();
       // wait for an interrupt that we've enabled as a wake source
       CpuSleepX86();
       INTERRUPTS_OFF();
       // determine what woke us up
       dwWake = OEMFindFirstWakeSource();
    } while (dwWake == SYSWAKE_UNKNOWN);
    
    // restore our interrupt mask
    SetPICInterruptMask(TRUE, bOldMaster);
    SetPICInterruptMask(FALSE, bOldSlave);

    dwLastWakeupSource = dwWake;
    
    INTERRUPTS_ON();
}


BOOL x86PowerIoctl (
    UINT32 code, VOID *lpInBuf, UINT32 nInBufSize, VOID *lpOutBuf, 
    UINT32 nOutBufSize, UINT32 *lpBytesReturned
) {
    switch (code) {
    case IOCTL_HAL_ENABLE_WAKE:
    case IOCTL_HAL_DISABLE_WAKE:
        if (lpInBuf && nInBufSize == sizeof(DWORD)) {
            DWORD dwReturn =
                ((code==IOCTL_HAL_ENABLE_WAKE)
                ? OEMSetWakeupSource   (*(PDWORD)lpInBuf)
                : OEMResetWakeupSource (*(PDWORD)lpInBuf));
            if (dwReturn)
                return TRUE;
        }
        break;
    case IOCTL_HAL_GET_WAKE_SOURCE:
        if (lpOutBuf && nOutBufSize>=sizeof(DWORD)) {
            *(PDWORD)lpOutBuf=OEMGetWakeupSource();
            if (lpBytesReturned)
                *lpBytesReturned=sizeof(DWORD);
            return TRUE;
        }
        break;
    case IOCTL_HAL_PRESUSPEND:
        OEMClearIntSources();
        return TRUE;
    default:
        break;
    }
    NKSetLastError (ERROR_INVALID_PARAMETER);
    return FALSE;
}
