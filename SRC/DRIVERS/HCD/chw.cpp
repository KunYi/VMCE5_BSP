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
//     CHW.cpp
// Abstract:
//     This file implements the UHCI specific register routines
//
// Notes:
//
//

#include "chw.hpp"
#include "cpipe.hpp"
#include "Cuhcd.hpp"
#include <nkintr.h>

#ifndef _PREFAST_
#pragma warning(disable: 4068) // Disable pragma warnings
#endif

CHW::CHW( IN const REGISTER portBase,
                              IN const DWORD dwSysIntr,
                              IN CPhysMem * const pCPhysMem,
                              //IN CUhcd * const pHcd,
                              IN LPVOID pvUhcdPddObject )
{
// definitions for static variables
    DEBUGMSG( ZONE_INIT, (TEXT("+CHW::CHW base=0x%x, intr=0x%x\n"), portBase, dwSysIntr));
    g_fPowerUpFlag = FALSE;
    g_fPowerResuming = FALSE;
    m_portBase = portBase;
    //m_pHcd = pHcd;
    m_pMem = pCPhysMem;
    m_pPddContext = pvUhcdPddObject;
    m_frameCounterHighPart = 0;
    m_frameCounterLowPart = 0;
    m_pFrameList = 0;

    m_dwSysIntr = dwSysIntr;
    m_hUsbInterruptEvent = NULL;
    m_hUsbInterruptThread = NULL;
    m_fUsbInterruptThreadClosing = FALSE;

    m_fFrameLengthIsBeingAdjusted = FALSE;
    m_fStopAdjustingFrameLength = FALSE;
    m_hAdjustDoneCallbackEvent = NULL;
    m_uNewFrameLength = 0;
    m_dwCapability = 0;
    m_bDoResume=FALSE;
}
//extern BOOL g_fPowerUpFlag;
//extern BOOL g_fPowerResuming;

// ******************************************************************
BOOL CHW::Initialize( )
// Purpose: Reset and Configure the Host Controller with the schedule.
//
// Parameters: portBase - base address for host controller registers
//
//             dwSysIntr - system interrupt number to use for USB
//                         interrupts from host controller
//
//             frameListPhysAddr - physical address of frame list index
//                                 maintained by CPipe class
//
//             pvUhcdPddObject - PDD specific structure used during suspend/resume
//
// Returns: TRUE if initialization succeeded, else FALSE
//
// Notes: This function is only called from the CUhcd::Initialize routine.
//
//        This function is static
// ******************************************************************
{
    DEBUGMSG( ZONE_INIT, (TEXT("+CHW::Initialize\n")));

    DEBUGCHK( m_frameCounterLowPart == 0 &&
              m_frameCounterHighPart == 0 );

    ULONG frameListPhysAddr;
    InitializeCriticalSection( &m_csFrameCounter );
    // set up the frame list area.
    if (m_pFrameList==NULL && m_pMem->AllocateSpecialMemory(FRAME_LIST_SIZE_IN_BYTES, (PUCHAR *) &m_pFrameList) == FALSE) {
        DEBUGMSG(ZONE_ERROR, (TEXT("-CHW::Initialize, cannot allocate the frame list!!\n")));
        m_pFrameList = 0;
        frameListPhysAddr = 0;
        return FALSE;
    }
    frameListPhysAddr = m_pMem->VaToPa((PUCHAR) m_pFrameList);

    if ( m_portBase == 0 || frameListPhysAddr == 0 ) {
        DEBUGMSG( ZONE_ERROR, (TEXT("-CHW::Initialize - zero Register Base or Frame List Address\n")));
        return FALSE;
    }

    // UHCI spec 2.1.6 - save SOFMOD before resetting HC
    UCHAR  savedSOFMOD = Read_SOFMOD() & UHCD_SOFMOD_MASK;

    // Signal Global Reset - do this ***BEFORE*** any other USB I/O register writing,
    // since Global Reset will cause all registers to revert to their hardware-reset
    // state.
    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS, (TEXT("CHW::Initialize - signalling global reset\n")));

    DEBUGMSG(ZONE_INIT, (TEXT("CHW::Initialize - Read_USBCMD 1 = %x\n"), Read_USBCMD()));
    Write_USBCMD( Read_USBCMD() | UHCD_USBCMD_GLOBAL_RESET );
    Sleep( 20 );
    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS, (TEXT("CHW::Initialize - end signalling global reset\n")));
    DEBUGMSG(ZONE_INIT, (TEXT("CHW::Initialize - Read_USBCMD 2 = %x\n"), Read_USBCMD()));
    Write_USBCMD( Read_USBCMD() & ~UHCD_USBCMD_GLOBAL_RESET );

    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS, (TEXT("CHW::Initialize - setting USBINTR to all interrupts on\n")));
    // initialize interrupt register - set all interrupts to enabled
    DEBUGMSG(ZONE_INIT, (TEXT("CHW::Initialize - Read_USBINTR 3 = %x\n"), Read_USBINTR()));
    Write_USBINTR( UHCD_USBINTR_SHORT_PACKET_INTERRUPT
                   | UHCD_USBINTR_INTERRUPT_ON_COMPLETE
                   | UHCD_USBINTR_RESUME_INTERRUPT
                   | UHCD_USBINTR_TIMEOUT_CRC_INTERRUPT );

    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("CHW::Initialize - setting FRNUM = 0\n")));
    DEBUGMSG(ZONE_INIT, (TEXT("CHW::Initialize - Read_USBINTR 4 = %x\n"), Read_USBINTR()));

    // initialize FRNUM register with index 0 of frame list
    Write_FRNUM( 0x0000 );

    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("CHW::Initialize - setting FLBASEADD = 0x%X\n"), frameListPhysAddr));
    // initialize FLBASEADD with address of frame list
    DEBUGCHK( frameListPhysAddr != 0 );
    // frame list should be aligned on a 4Kb boundary
    DEBUGCHK( (frameListPhysAddr & UHCD_FLBASEADD_MASK) == frameListPhysAddr );
    Write_FLBASEADD( frameListPhysAddr );

    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("CHW::Initialize - setting SOFMOD to 0x%x\n"), savedSOFMOD ));
    Write_SOFMOD( savedSOFMOD );

    // m_hUsbInterrupt - Auto Reset, and Initial State = non-signaled
    DEBUGCHK( m_hUsbInterruptEvent == NULL );
    m_hUsbInterruptEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
    if ( m_hUsbInterruptEvent == NULL ) {
        DEBUGMSG(ZONE_ERROR, (TEXT("-CHW::Initialize. Error creating USBInterrupt event\n")));
        return FALSE;
    }

#if DEBUG
	DumpAllRegisters();
#endif

    InterruptDisable( m_dwSysIntr ); // Just to make sure this is really ours.
    // Initialize Interrupt. When interrupt id # m_sysIntr is triggered,
    // m_hUsbInterruptEvent will be signaled. Last 2 params must be NULL
    if ( !InterruptInitialize( m_dwSysIntr, m_hUsbInterruptEvent, NULL, NULL) ) {
        DEBUGMSG(ZONE_ERROR, (TEXT("-CHW::Initialize. Error on InterruptInitialize\r\n")));
        return FALSE;
    }
    // Start up our IST - the parameter passed to the thread
    // is unused for now
    DEBUGCHK( m_hUsbInterruptThread == NULL &&
              m_fUsbInterruptThreadClosing == FALSE );
    if (m_hUsbInterruptThread==NULL)
        m_hUsbInterruptThread = CreateThread( 0, 0, UsbInterruptThreadStub, this, 0, NULL );
    if ( m_hUsbInterruptThread == NULL ) {
        DEBUGMSG(ZONE_ERROR, (TEXT("-CHW::Initialize. Error creating IST\n")));
        return FALSE;
    }
    CeSetThreadPriority( m_hUsbInterruptThread, g_IstThreadPriority );
    DEBUGMSG( ZONE_INIT, (TEXT("-CHW::Initialize, success!\n")));
    return TRUE;
}
CHW::~CHW()
{
    if (m_dwSysIntr)
        KernelIoControl(IOCTL_HAL_DISABLE_WAKE, &m_dwSysIntr, sizeof(m_dwSysIntr), NULL, 0, NULL);
    DeInitialize();
}
// ******************************************************************
void CHW::DeInitialize( void )
//
// Purpose: Delete any resources associated with static members
//
// Parameters: none
//
// Returns: nothing
//
// Notes: This function is only called from the ~CUhcd() routine.
//
//        This function is static
// ******************************************************************
{
    m_fUsbInterruptThreadClosing = TRUE; // tell USBInterruptThread that we are closing
    // tell adjustment thread (if it exists) to close
    InterlockedExchange( &m_fStopAdjustingFrameLength, TRUE );

    if ( m_fFrameLengthIsBeingAdjusted ) {
        Sleep( 20 ); // give adjustment thread time to close
        DEBUGCHK( !m_fFrameLengthIsBeingAdjusted );
    }
    // m_hAdjustDoneCallbackEvent <- don't need to do anything to this
    // m_uNewFrameLength <- don't need to do anything to this

    // Wake up the interrupt thread and give it time to die.
    if ( m_hUsbInterruptEvent ) {
        SetEvent(m_hUsbInterruptEvent);
        if ( m_hUsbInterruptThread ) {
            DWORD dwWaitReturn = WaitForSingleObject(m_hUsbInterruptThread, 1000);
            if ( dwWaitReturn != WAIT_OBJECT_0 ) {
                DEBUGCHK( 0 );
#pragma prefast(suppress: 258, "Try to recover gracefully from a pathological failure")
                TerminateThread(m_hUsbInterruptThread, DWORD(-1));
            }
            CloseHandle(m_hUsbInterruptThread);
            m_hUsbInterruptThread = NULL;
        }
        // we have to close our interrupt before closing the event!
        InterruptDisable( m_dwSysIntr );

        CloseHandle(m_hUsbInterruptEvent);
        m_hUsbInterruptEvent = NULL;
    } else {
        InterruptDisable( m_dwSysIntr );
    }

    // no need to free the frame list; the entire pool will be freed as a unit.
    m_pFrameList = 0;

    DeleteCriticalSection( &m_csFrameCounter );

    m_fUsbInterruptThreadClosing = FALSE;
    m_frameCounterLowPart = 0;
    m_frameCounterHighPart = 0;
}

// ******************************************************************
void CHW::EnterOperationalState( void )
//
// Purpose: Signal the host controller to start processing the schedule
//
// Parameters: None
//
// Returns: Nothing.
//
// Notes: This function is only called from the CUhcd::Initialize routine.
//        It assumes that CPipe::Initialize and CHW::Initialize
//        have already been called.
//
//        This function is static
// ******************************************************************
{
    DEBUGMSG( ZONE_INIT, (TEXT("+CHW::EnterOperationalState\n")));

    USHORT usData;

    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("CHW::EnterOperationalState - clearing status reg\n")));
    Clear_USBSTS( );

    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("CHW::EnterOperationalState - setting USBCMD run bit\n")));
    usData = Read_USBCMD();
    // if reclamation size is 32, we shouldn't set the USBCMD bit
    DEBUGCHK( UHCD_MAX_RECLAMATION_PACKET_SIZE == 64 );
    Write_USBCMD( usData | UHCD_USBCMD_MAX_RECLAMATION_SIZE_64 | UHCD_USBCMD_RUN_STOP | UHCD_USBCMD_CONFIGURE_FLAG );

#if DEBUG
	DumpAllRegisters();
#endif
    DEBUGMSG( ZONE_INIT, (TEXT("-CHW::EnterOperationalState\n")));
}

// ******************************************************************
void CHW::StopHostController( void )
//
// Purpose: Signal the host controller to stop processing the schedule
//
// Parameters: None
//
// Returns: Nothing.
//
// Notes:
//
//        This function is static
// ******************************************************************
{
    if ( m_portBase != 0 ) {
        WORD wUSBCmd = Read_USBCMD();
        // Check run bit. Despite what the UHCI spec says, Intel's controller
        // does not always set the HCHALTED bit when the controller is stopped.
        if(wUSBCmd & UHCD_USBCMD_RUN_STOP)
            {
                // clear run bit
                Write_USBCMD( wUSBCmd & ~UHCD_USBCMD_RUN_STOP );
                // clear all interrupts
                Write_USBINTR( 0x0000 );
                // spin until the controller really is stopped
                while ( ! (Read_USBSTS() & UHCD_USBSTS_HCHALTED) )
                    ;
            }
    }
}
DWORD CALLBACK CHW::CeResumeThreadStub ( IN PVOID context )
{
    return ((CHW *)context)->CeResumeThread ( );
}
// ******************************************************************
DWORD CHW::CeResumeThread ( )
//
// Purpose: Force the HCD to reset and regenerate itself after power loss.
//
// Parameters: None
//
// Returns: Nothing.
//
// Notes: Because the PDD is probably maintaining pointers to the Hcd and Memory
//   objects, we cannot free/delete them and then reallocate. Instead, we destruct
//   them explicitly and use the placement form of the new operator to reconstruct
//   them in situ. The two flags synchronize access to the objects so that they
//   cannot be accessed before being reconstructed while also guaranteeing that
//   we don't miss power-on events that occur during the reconstruction.
//
//        This function is static
// ******************************************************************
{
    // reconstruct the objects at the same addresses where they were before;
    // this allows us not to have to alert the PDD that the addresses have changed.

    DEBUGCHK( g_fPowerResuming == FALSE );

    // order is important! resuming indicates that the hcd object is temporarily invalid
    // while powerup simply signals that a powerup event has occurred. once the powerup
    // flag is cleared, we will repeat this whole sequence should it get resignalled.
    g_fPowerUpFlag = FALSE;
    g_fPowerResuming = TRUE;

    const PUCHAR pBufVirt = m_pMem->m_pVirtBase, pBufPhys = m_pMem->m_pPhysBase;
    DWORD cb0 = m_pMem->m_cbTotal, cb1 = m_pMem->m_cbHighPri;

    DeviceDeInitialize();
    while (1) {  // breaks out upon successful reinit of the object

        m_pMem->ReInit();
        if (DeviceInitialize())
            break;
        // getting here means we couldn't reinit the HCD object!
        DEBUGMSG(ZONE_ERROR, (TEXT("USB cannot reinit the HCD at CE resume; retrying...\n")));
        DeviceDeInitialize();
        Sleep(15000);
    }

    // the hcd object is valid again. if a power event occurred between the two flag
    // assignments above then the IST will reinitiate this sequence.
    g_fPowerResuming = FALSE;
    if (g_fPowerUpFlag)
        PowerMgmtCallback(TRUE);
    
    return 0;
}
DWORD CHW::UsbInterruptThreadStub( IN PVOID context )
{
    return ((CHW *)context)->UsbInterruptThread();
}

// ******************************************************************
DWORD CHW::UsbInterruptThread( )
//
// Purpose: Main IST to handle interrupts from the USB host controller
//
// Parameters: context - parameter passed in when starting thread,
//                       (currently unused)
//
// Returns: 0 on thread exit.
//
// Notes:
//
//        This function is private
// ******************************************************************
{
    DEBUGMSG(ZONE_INIT && ZONE_VERBOSE, (TEXT("+CHW::Entered USBInterruptThread\n")));

    while ( !m_fUsbInterruptThreadClosing ) {
        WaitForSingleObject(m_hUsbInterruptEvent, INFINITE);
        if ( m_fUsbInterruptThreadClosing ) {
            break;
        }

        USHORT usbsts = Read_USBSTS();
    #ifdef DEBUG
        DWORD dwFrame;
        GetFrameNumber(&dwFrame); // calls UpdateFrameCounter
        DEBUGMSG( ZONE_REGISTERS, (TEXT("!!!interrupt!!!! on frame index + 1 = 0x%08x, USBSTS = 0x%04x\n"), dwFrame, usbsts ) );
    #else
        UpdateFrameCounter();
    #endif // DEBUG

        Clear_USBSTS( );

        // TODO - differentiate between USB interrupts, which are
        // for transfers, and host interrupts (UHCI spec 2.1.2).
        // For the former, we need to call CPipe::SignalCheckForDoneTransfers.
        // For the latter, we need to call whoever will handle
        // resume/error processing.

        // For now, we just notify CPipe so that transfers
        // can be checked for completion

        // This flag gets cleared in the resume thread.
        if(g_fPowerUpFlag)
        {
            if (m_bDoResume) {
                g_fPowerUpFlag=FALSE;
                // UHCI 2.1.1
                WORD wUSBCmd = Read_USBCMD();
                Sleep(20); 
                Write_USBCMD(wUSBCmd & ~UHCD_USBCMD_FORCE_GLOBAL_RESUME);
            }
            else {
                if (g_fPowerResuming) {
                    // this means we've restarted an IST and it's taken an early interrupt;
                    // just pretend it didn't happen for now because we're about to be told to exit again.
                    continue;
                }
                HcdPdd_InitiatePowerUp((DWORD) m_pPddContext);
                HANDLE ht;
                while ((ht = CreateThread(NULL, 0, CeResumeThreadStub, this, 0, NULL)) == NULL) {
                    RETAILMSG(1, (TEXT("HCD IST: cannot spin a new thread to handle CE resume of USB host controller; sleeping.\n")));
                    Sleep(15000);  // 15 seconds later, maybe it'll work.
                }
                CeSetThreadPriority( ht, g_IstThreadPriority );
                CloseHandle(ht);
                
                // The CE resume thread will force this IST to exit so we'll be cooperative proactively.
                break;
            }
        }
        else {
            if ((usbsts & UHCD_USBSTS_RESUME_DETECT )!=0)  { // UHCI 2.1.2
                WORD wUSBCmd = Read_USBCMD();
                if (wUSBCmd & UHCD_USBCMD_ENTER_GLOBAL_SUSPEND_MODE) {
                    ResumeHostController();
                    CHcd::ResumeNotification();
                };
            }
            
            SignalCheckForDoneTransfers( );
        }

        InterruptDone(m_dwSysIntr);
    }

    DEBUGMSG(ZONE_INIT && ZONE_VERBOSE, (TEXT("-CHW::Leaving USBInterruptThread\n")));

    return (0);
}

// ******************************************************************
void CHW::UpdateFrameCounter( void )
//
// Purpose: Updates our internal frame counter
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: The UHCI frame number register is only 11 bits, or 2047
//        long. Thus, the counter will wrap approx. every 2 seconds.
//        That is insufficient for Isoch Transfers, which
//        may need to be scheduled out into the future by more
//        than 2 seconds. So, we maintain an internal 32 bit counter
//        for the frame number, which will wrap in 50 days.
//
//        This function should be called at least once every two seconds,
//        otherwise we will miss frames.
//
// ******************************************************************
{
#ifdef DEBUG
    DWORD dwTickCountLastTime = GetTickCount();
#endif

    EnterCriticalSection( &m_csFrameCounter );

#ifdef DEBUG
    // If this fails, we haven't been called in a long time,
    // so the frame number is no longer accurate
    if (GetTickCount() - dwTickCountLastTime >= 2000 )
        DEBUGMSG(1, (TEXT("!UHCI - CHW::UpdateFrameCounter missed frame count;")
                     TEXT(" isoch packets may have been dropped.\n")));
    dwTickCountLastTime = GetTickCount();
#endif // DEBUG

    // This algorithm is right out of the Win98 uhcd.c code
    USHORT currentFRNUM = Read_FRNUM() & UHCD_FRNUM_MASK;
    // check whether the MSB in m_frameCounterLowPart and currentFRNUM differ
    if ( (m_frameCounterLowPart ^ currentFRNUM) & UHCD_FRNUM_COUNTER_MSB ) {
        // Yes, they are different. Update m_frameCounterHighPart
        m_frameCounterHighPart += (UHCD_FRNUM_COUNTER_MSB << 1) -
                ((currentFRNUM ^ m_frameCounterHighPart) & UHCD_FRNUM_COUNTER_MSB);
    }
    m_frameCounterLowPart = currentFRNUM;

    LeaveCriticalSection( &m_csFrameCounter );
}

// ******************************************************************
BOOL CHW::GetFrameNumber( OUT LPDWORD lpdwFrameNumber )
//
// Purpose: Return the current frame number
//
// Parameters: None
//
// Returns: 32 bit current frame number
//
// Notes: See also comment in UpdateFrameCounter
// ******************************************************************
{
    EnterCriticalSection( &m_csFrameCounter );

    // This algorithm is right out of the Win98 uhcd.c code
    UpdateFrameCounter();
    DWORD frame = ((m_frameCounterLowPart & UHCD_FRNUM_INDEX_MASK) | m_frameCounterHighPart) +
                    ((m_frameCounterLowPart ^ m_frameCounterHighPart) & UHCD_FRNUM_COUNTER_MSB);

    LeaveCriticalSection( &m_csFrameCounter );

    *lpdwFrameNumber=frame;
    return TRUE;
}

// ******************************************************************
BOOL CHW::WaitOneFrame( void )
//
// Purpose: Block the current thread until the HC hardware is
//          no longer processing the current USB frame.
//
// Parameters: None
//
// Returns: TRUE on success, FALSE if the HW is unavailable or not running.
//
// Notes:
// ******************************************************************
{
#ifdef CE_PREv3
    // Use the hardware, Luke!
    // The OS' system clock (used for scheduling) has 25ms granularity
    // which is just too high.
    if ((Read_USBCMD() & UHCD_USBCMD_RUN_STOP) == 0)
        // We check the USBCMD register instead of the USBSTS register because
        // Intel's HC can be stopped without the HCHALTED bit being set.
        return FALSE;

    DWORD frame0 = GetFrameNumber();

    while (GetFrameNumber() == frame0)
        ; // spin

#else // 3.0 and later use 1ms system clock
    // This isn't totally accurate (e.g., when the frame length changes)
    // but it's close enough and keeps us from spinning.
    Sleep(1);

#endif

    return TRUE;
}

// ******************************************************************
BOOL CHW::GetFrameLength( OUT LPUSHORT lpuFrameLength )
//
// Purpose: Return the current frame length in 12 MHz clocks
//          (i.e. 12000 = 1ms)
//
// Parameters: None
//
// Returns: frame length
//
// Notes: Only part of the frame length is stored in the hardware
//        register, so an offset needs to be added.
// ******************************************************************
{
    *lpuFrameLength=(Read_SOFMOD() & UHCD_SOFMOD_MASK) + UHCD_SOFMOD_MINIMUM_LENGTH;
    return TRUE;
}

// ******************************************************************
BOOL CHW::SetFrameLength( IN HANDLE hEvent,
                          IN USHORT uFrameLength )
//
// Purpose: Set the Frame Length in 12 Mhz clocks. i.e. 12000 = 1ms
//
// Parameters:  hEvent - event to set when frame has reached required
//                       length
//
//              uFrameLength - new frame length
//
// Returns: TRUE if frame length changed, else FALSE
//
// Notes:
// ******************************************************************
{
    BOOL fSuccess = FALSE;

    // to prevent multiple threads from simultaneously adjusting the
    // frame length, InterlockedTestExchange is used. This is
    // cheaper than using a critical section.
    if ( FALSE == InterlockedTestExchange( &m_fFrameLengthIsBeingAdjusted,
                                           FALSE, // Test value (Old value)
                                           TRUE ) ) { // New value

        // m_fFrameLengthIsBeingAdjusted was set to TRUE
        // by the InterlockedTestExchange
        if ( uFrameLength >= UHCD_SOFMOD_MINIMUM_LENGTH &&
             uFrameLength <= UHCD_SOFMOD_MAXIMUM_LENGTH &&
             hEvent != NULL ) {

            // ok, all the params are fine
            m_hAdjustDoneCallbackEvent = hEvent;
            m_uNewFrameLength = uFrameLength;
            InterlockedExchange( &m_fStopAdjustingFrameLength, FALSE );

            // the frame length needs to be adjusted over
            // many frames, so we need a separate thread

            HANDLE hWorkerThread = CreateThread( 0, 0, UsbAdjustFrameLengthThreadStub, this, 0, NULL );
            if ( hWorkerThread != NULL ) {
                CeSetThreadPriority( hWorkerThread, g_IstThreadPriority + RELATIVE_PRIO_ADJUST_FRAME );
                CloseHandle( hWorkerThread );
                hWorkerThread = NULL;
                fSuccess = TRUE;
            }
        }
        if ( !fSuccess ) {
            // we didn't succeed, so change m_fFrameLengthIsBeingAdjusted
            // back to FALSE
        #ifdef DEBUG
            LONG oldValue =
        #endif // DEBUG
                InterlockedExchange( &m_fFrameLengthIsBeingAdjusted, FALSE );
            DEBUGCHK( oldValue );
        }
    }
    return fSuccess;
}

// ******************************************************************
BOOL CHW::StopAdjustingFrame( void )
//
// Purpose: Stop modifying the host controller frame length
//
// Parameters: None
//
// Returns: TRUE
//
// Notes:
// ******************************************************************
{
    InterlockedExchange( &m_fStopAdjustingFrameLength, TRUE );
    return TRUE;
}
DWORD CHW::UsbAdjustFrameLengthThreadStub(IN PVOID context )
{
    return ((CHW *)context)->UsbAdjustFrameLengthThread();
}
// ******************************************************************
DWORD CHW::UsbAdjustFrameLengthThread( )
//
// Purpose: Worker thread to handle frame length adjustment
//
// Parameters: context - parameter passed in when starting thread,
//                       (currently unused)
//
// Returns: 0 on thread exit.
//
// Notes:
//
//        This function is private
// ******************************************************************
{
    DEBUGMSG(ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("+CHW::Entered UsbAdjustFrameLengthThread\n")));

    DEBUGCHK( m_fFrameLengthIsBeingAdjusted &&
              !m_fStopAdjustingFrameLength &&
              m_hAdjustDoneCallbackEvent != NULL &&
              m_uNewFrameLength >= UHCD_SOFMOD_MINIMUM_LENGTH &&
              m_uNewFrameLength <= UHCD_SOFMOD_MAXIMUM_LENGTH );

    const UCHAR uRequiredSOFMOD = UCHAR( m_uNewFrameLength - UHCD_SOFMOD_MINIMUM_LENGTH );

    // if the interrupt thread is closing, or we're signaled
    // to stop adjusting the frame length, stop!
    while ( !m_fStopAdjustingFrameLength &&
            !m_fUsbInterruptThreadClosing ) {

        UCHAR uCurrentSOFMOD = Read_SOFMOD() & UHCD_SOFMOD_MASK;
        DEBUGMSG(ZONE_REGISTERS, (TEXT("CHW::UsbAdjustFrameLengthThread, current length = %d, required = %d\n"), UHCD_SOFMOD_MINIMUM_LENGTH + uCurrentSOFMOD, UHCD_SOFMOD_MINIMUM_LENGTH + uRequiredSOFMOD ));
        if ( uCurrentSOFMOD == uRequiredSOFMOD ) {
            // ok, we're done
#pragma prefast(disable: 322, "Recover gracefully from hardware failure")
            __try {
                SetEvent( m_hAdjustDoneCallbackEvent );
            } __except(EXCEPTION_EXECUTE_HANDLER) {
            }
#pragma prefast(pop)
            break;
        } else if ( uCurrentSOFMOD > uRequiredSOFMOD ) {
            uCurrentSOFMOD--;
        } else {
            DEBUGCHK( uCurrentSOFMOD < uRequiredSOFMOD );
            uCurrentSOFMOD++;
        }
        // USB spec 1.1, section 7.1.12 says we can't adjust
        // the frame length by more than 1 bit every 6 frames.
        // Assuming a maximum frame length of 12063 bits, plus
        // 15 bits error, that totals 6.039ms. So, just to
        // be safe, we'll sleep 7 ms before adjusting the length.
        Sleep( 7 );

        Write_SOFMOD( uCurrentSOFMOD );
    }
    // set m_fFrameLengthIsBeingAdjusted to FALSE
    InterlockedExchange( &m_fFrameLengthIsBeingAdjusted, FALSE );

    DEBUGMSG(ZONE_REGISTERS, (TEXT("-CHW::UsbAdjustFrameLengthThread\n")));

    return (0);
}

// ******************************************************************
BOOL CHW::DidPortStatusChange( IN const UCHAR port )
//
// Purpose: Determine whether the status of root hub port # "port" changed
//
// Parameters: port - 0 for the hub itself, otherwise the hub port number
//
// Returns: TRUE if status changed, else FALSE
//
// Notes:
// ******************************************************************
{
    // port == specifies root hub itself, whose status never changes
    WORD wChanged = 0;
    if ( port > 0 ) {
        const USHORT portsc = Read_PORTSC( port );
        wChanged = portsc & (UHCD_PORTSC_CONNECT_STATUS_CHANGE | UHCD_PORTSC_PORT_ENABLE_CHANGE|UHCD_PORTSC_RESUME_DETECT);
    }
    return !!wChanged;
}

// ******************************************************************
BOOL CHW::GetPortStatus( IN const UCHAR port,
                         OUT USB_HUB_AND_PORT_STATUS& rStatus )
//
// Purpose: This function will return the current root hub port
//          status in a non-hardware specific format
//
// Parameters: port - 0 for the hub itself, otherwise the hub port number
//
//             rStatus - reference to USB_HUB_AND_PORT_STATUS to get the
//                       status
//
// Returns: TRUE
//
// Notes:
// ******************************************************************
{
    memset( &rStatus, 0, sizeof( USB_HUB_AND_PORT_STATUS ) );
    if ( port > 0 ) {
        // request refers to a root hub port

        // read the port status register
        const USHORT portsc = Read_PORTSC( port );
        
        // Now fill in the USB_HUB_AND_PORT_STATUS structure
        rStatus.change.port.ConnectStatusChange = !!(portsc & UHCD_PORTSC_CONNECT_STATUS_CHANGE);
        rStatus.change.port.PortEnableChange = !!(portsc & UHCD_PORTSC_PORT_ENABLE_CHANGE);
        // for root hub, we don't set any of these change bits:
        DEBUGCHK( rStatus.change.port.OverCurrentChange == 0 );
        DEBUGCHK( rStatus.change.port.ResetChange ==0);
        rStatus.status.port.DeviceIsLowSpeed = !!(portsc & UHCD_PORTSC_LOW_SPEED_DEVICE);
        rStatus.status.port.PortConnected = !!(portsc & UHCD_PORTSC_CONNECT_STATUS);
        rStatus.status.port.PortEnabled = !!(portsc & UHCD_PORTSC_PORT_ENABLED);
        // no root port over current indicators in UHCI
        DEBUGCHK( rStatus.status.port.PortOverCurrent == 0 );
        // root hub ports are always powered
        rStatus.status.port.PortPower = 1;
        rStatus.status.port.PortReset = !!(portsc & UHCD_PORTSC_PORT_RESET);
        rStatus.status.port.PortSuspended = !!(portsc & UHCD_PORTSC_SUSPEND);
        if (portsc & UHCD_PORTSC_RESUME_DETECT) { // If we do not clear this bit . port will not really resume 2.1.7
            rStatus.change.port.SuspendChange = 1;
            rStatus.status.port.PortSuspended = 0;            
            Write_PORTSC(port, portsc & ~(UHCD_PORTSC_SUSPEND|  UHCD_PORTSC_RESUME_DETECT| UHCD_PORTSC_CONNECT_STATUS_CHANGE | UHCD_PORTSC_PORT_ENABLE_CHANGE));
        }
    }
#ifdef DEBUG
    else {
        // request is to Hub. rStatus was already memset to 0 above.
        DEBUGCHK( port == 0 );
        // local power supply good
        DEBUGCHK( rStatus.status.hub.LocalPowerStatus == 0 );
        // no over current condition
        DEBUGCHK( rStatus.status.hub.OverCurrentIndicator == 0 );
        // no change in power supply status
        DEBUGCHK( rStatus.change.hub.LocalPowerChange == 0 );
        // no change in over current status
        DEBUGCHK( rStatus.change.hub.OverCurrentIndicatorChange == 0 );
    }
	DumpAllRegisters();
#endif // DEBUG

    return TRUE;
}

// ******************************************************************
BOOL CHW::RootHubFeature( IN const UCHAR port,
                          IN const UCHAR setOrClearFeature,
                          IN const USHORT feature )
//
// Purpose: This function clears all the status change bits associated with
//          the specified root hub port.
//
// Parameters: port - 0 for the hub itself, otherwise the hub port number
//
// Returns: TRUE iff the requested operation is valid, FALSE otherwise.
//
// Notes: Assume that caller has already verified the parameters from a USB
//        perspective. The HC hardware may only support a subset of that
//        (which is indeed the case for UHCI).
// ******************************************************************
{
    if (port == 0) {
        // request is to Hub but...
        // uhci has no way to tweak features for the root hub.
        return FALSE;
    }

    // mask the change bits because we write 1 to them to clear them //
    USHORT bmStatus = Read_PORTSC( port );
    bmStatus &= ~(UHCD_PORTSC_CONNECT_STATUS_CHANGE | UHCD_PORTSC_PORT_ENABLE_CHANGE);

    if (setOrClearFeature == USB_REQUEST_SET_FEATURE)
        switch (feature) {
          case USB_HUB_FEATURE_PORT_RESET:              bmStatus |= UHCD_PORTSC_PORT_RESET; break;
          case USB_HUB_FEATURE_PORT_SUSPEND:            bmStatus |= UHCD_PORTSC_SUSPEND;  break;
          case USB_HUB_FEATURE_PORT_POWER:              // not supported by UHCI //
          default: return FALSE;
        }
    else
        switch (feature) {
          case USB_HUB_FEATURE_PORT_ENABLE:     bmStatus &= ~UHCD_PORTSC_PORT_ENABLED; break;
          case USB_HUB_FEATURE_PORT_SUSPEND:        bmStatus &= ~UHCD_PORTSC_SUSPEND; break;
          case USB_HUB_FEATURE_C_PORT_CONNECTION:       bmStatus |= UHCD_PORTSC_CONNECT_STATUS_CHANGE; break;
          case USB_HUB_FEATURE_C_PORT_ENABLE:           bmStatus |= UHCD_PORTSC_PORT_ENABLE_CHANGE; break;
          case USB_HUB_FEATURE_C_PORT_RESET:            bmStatus &= ~UHCD_PORTSC_PORT_RESET; break;            
          case USB_HUB_FEATURE_C_PORT_SUSPEND:
            if (bmStatus & UHCD_PORTSC_SUSPEND) { // If in Suspend Status. Rise resume to resume offstream device.
                bmStatus |= UHCD_PORTSC_RESUME_DETECT;
                bmStatus &= ~UHCD_PORTSC_SUSPEND;
                Write_PORTSC( port, bmStatus );
                Sleep(20);
                bmStatus &= ~(UHCD_PORTSC_RESUME_DETECT | UHCD_PORTSC_SUSPEND);
            }
            break;            
          case USB_HUB_FEATURE_C_PORT_OVER_CURRENT:
          case USB_HUB_FEATURE_PORT_POWER:
          default: return FALSE;
        }

    Write_PORTSC( port, bmStatus );
    return TRUE;
}

// ******************************************************************
BOOL CHW::ResetAndEnablePort( IN const UCHAR port )
//
// Purpose: reset/enable device on the given port so that when this
//          function completes, the device is listening on address 0
//
// Parameters: port - root hub port # to reset/enable
//
// Returns: TRUE if port reset and enabled, else FALSE
//
// Notes: This function takes approx 60 ms to complete, and assumes
//        that the caller is handling any critical section issues
//        so that two different ports (i.e. root hub or otherwise)
//        are not reset at the same time.
// ******************************************************************
{
    BOOL fSuccess = FALSE;

    USHORT portsc = Read_PORTSC( port );
    // no point reset/enabling the port unless something is attached
    if ( portsc & UHCD_PORTSC_CONNECT_STATUS ) {
        // turn on reset bit
        portsc |= UHCD_PORTSC_PORT_RESET;
        Write_PORTSC( port, portsc );
        // Note - Win98 waited 10 ms here. But, the new USB 1.1 spec
        // section 7.1.7.3 recommends 50ms for root hub ports
        Sleep( 50 );

        // Clear the reset bit
        portsc = Read_PORTSC( port );
        portsc &= ~UHCD_PORTSC_PORT_RESET;
        Write_PORTSC( port, portsc );

        // Reset is complete, enable the port
        //
        // What I used to do - clear the reset bit, wait 10ms,
        // set the enabled bit, wait 10 ms. This worked with
        // the MS mouse, NEC hub and Andromeda hub. However,
        // with the Peracom hub, it wouldn't!! SetAddress would
        // fail on the Peracom hub for some weird reason. I lucked out
        // by finding the following in the Win98 uhcd code. It is
        // very strange, but this loop fixes the problem! More
        // strange is that every device seems to execute the
        // loop 3-5 times. Perhaps the ENABLED bit is flaky
        // during the period just after reset?? I would have expected
        // to just set the ENABLED bit and have it stick, but
        // apparently the hardware is not that nice.
        //
        // In the Win98 code, this loop was marked with the comment:
        // "BUGBUG not sure why we need this loop
        // original code from intel has this"
        for ( UCHAR i = 1; i < 10; i++ ) {
            portsc = Read_PORTSC( port );
            if ( portsc & UHCD_PORTSC_PORT_ENABLED ) {
                // port is enabled
                fSuccess = TRUE;
                break;
            }
            // enable the port
            portsc |= UHCD_PORTSC_PORT_ENABLED;
            Write_PORTSC( port, portsc );
        }
        //
        // clear port connect & enable change bits
        //
        // the process of reset/enable has somehow set the connect
        // change bit again. If we don't clear it, we'll be attaching
        // this device infinitely many times. (note these bits are
        // write-clear).
        //
        portsc |= UHCD_PORTSC_CONNECT_STATUS_CHANGE | UHCD_PORTSC_PORT_ENABLE_CHANGE;
        Write_PORTSC( port, portsc );

        // USB 1.1 spec, 7.1.7.3 - device may take up to 10 ms
        // to recover after reset is removed
        Sleep( 10 );
    }

#if DEBUG
	DumpAllRegisters();
#endif
    DEBUGMSG( ZONE_REGISTERS, (TEXT("Root hub, after reset & enable, port %d portsc = 0x%04x\n"), port, Read_PORTSC( port ) ) );
    return fSuccess;
}

// ******************************************************************
void CHW::DisablePort( IN const UCHAR port )
//
// Purpose: disable the given root hub port
//
// Parameters: port - port # to disable
//
// Returns: nothing
//
// Notes: This function will take about 10ms to complete
// ******************************************************************
{
    USHORT portsc = Read_PORTSC( port );
    // no point doing any work unless the port is enabled
    if ( portsc & UHCD_PORTSC_PORT_ENABLED ) {
        // clear port enabled bit and enabled change bit,
        // but don't alter the connect status change bit,
        // which is write-clear.
        portsc &= ~(UHCD_PORTSC_PORT_ENABLED | UHCD_PORTSC_CONNECT_STATUS_CHANGE);
        portsc |= UHCD_PORTSC_PORT_ENABLE_CHANGE;

        Write_PORTSC( port, portsc );

        // disable port can take some time to act, because
        // a USB request may have been in progress on the port.
        Sleep( 10 );
    }
}
// ******************************************************************
VOID CHW::PowerMgmtCallback( IN BOOL fOff )
//
// Purpose: System power handler - called when device goes into/out of
//          suspend.
//
// Parameters:  fOff - if TRUE indicates that we're entering suspend,
//                     else signifies resume
//
// Returns: Nothing
//
// Notes: This needs to be implemented for HCDI
// ******************************************************************
{
    if ( fOff )
    {
        if ((GetCapability() & HCD_SUSPEND_RESUME)!= 0) {
            m_bDoResume=TRUE;
            SuspendHostController();
        }
        else {
            m_bDoResume=FALSE;;
            CHW::StopHostController();
        }
    }
    else
    {   // resuming...
        g_fPowerUpFlag = TRUE;
        if (m_bDoResume)
            ResumeHostController();
        if (!g_fPowerResuming)
            // can't use member data while `this' is invalid
            SetInterruptEvent(m_dwSysIntr);
    }
    return;
}
VOID CHW::SuspendHostController()
{
    if ( m_portBase != 0 ) {
        // initialize interrupt register - set only RESUME interrupts to enabled
        WORD wUSBCmd = Read_USBCMD();
        wUSBCmd &= ~UHCD_USBCMD_RUN_STOP;
        Write_USBCMD(wUSBCmd);
        Write_USBCMD(wUSBCmd | UHCD_USBCMD_ENTER_GLOBAL_SUSPEND_MODE);
    }
}
VOID CHW::ResumeHostController()
{
    if ( m_portBase != 0 ) {
        WORD wUSBCmd = Read_USBCMD();
        wUSBCmd |= UHCD_USBCMD_FORCE_GLOBAL_RESUME;
        Write_USBCMD(wUSBCmd);
        // I need 20 ms delay here 30(30ns)*1000*20
        for (DWORD dwIndex =0; dwIndex<30*1000*20; dwIndex++)
            Read_USBCMD();
        wUSBCmd &= ~(UHCD_USBCMD_FORCE_GLOBAL_RESUME | UHCD_USBCMD_ENTER_GLOBAL_SUSPEND_MODE);
        wUSBCmd |= UHCD_USBCMD_RUN_STOP;
        Write_USBCMD(wUSBCmd);
    }
    ResumeNotification();
}
DWORD CHW::SetCapability(DWORD dwCap)
{
    m_dwCapability |= dwCap; 
    if ( (m_dwCapability & HCD_SUSPEND_RESUME)!=0) {
        KernelIoControl(IOCTL_HAL_ENABLE_WAKE, &m_dwSysIntr, sizeof(m_dwSysIntr), NULL, 0, NULL);
    }
    return m_dwCapability;
};

BOOL CHW::SuspendHC()
{
    BOOL fReturn = FALSE;
    if ((GetCapability() & (HCD_SUSPEND_ON_REQUEST | HCD_SUSPEND_RESUME)) == (HCD_SUSPEND_ON_REQUEST | HCD_SUSPEND_RESUME)) {
        SuspendHostController();
        fReturn = TRUE;
    }
    return fReturn;
}

#ifdef DEBUG
// ******************************************************************
void CHW::DumpUSBCMD( void )
//
// Purpose: Queries Host Controller for contents of USBCMD, and prints
//          them to DEBUG output. Bit definitions are in UHCI spec 2.1.1
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: used in DEBUG mode only
//
//        This function is static
// ******************************************************************
{
    DWORD    dwData = 0;

    __try {
        dwData = Read_USBCMD();

        DEBUGMSG(ZONE_REGISTERS, (TEXT("\tCHW - USB COMMAND REGISTER (USBCMD) = 0x%X. Dump:\n"), dwData));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tMax Reclamation Packet size = %s\n"), ((dwData & UHCD_USBCMD_MAX_RECLAMATION_SIZE_64) ? TEXT("64 bytes") : TEXT("32 bytes"))));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tConfigure Flag = %s\n"), ((dwData & UHCD_USBCMD_CONFIGURE_FLAG) ? TEXT("Configured") : TEXT("Not Configured"))));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tSoftware Debug = %s\n"), ((dwData & UHCD_USBCMD_SOFTWARE_DEBUG) ? TEXT("Debug Mode") : TEXT("Normal Mode"))));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tForce Global Resume = %s\n"), ((dwData & UHCD_USBCMD_FORCE_GLOBAL_RESUME) ? TEXT("Set") : TEXT("Not Set"))));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tEnter Global Suspend Mode = %s\n"), ((dwData & UHCD_USBCMD_ENTER_GLOBAL_SUSPEND_MODE) ? TEXT("Set") : TEXT("Not Set"))));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tGlobal Reset = %s\n"), ((dwData & UHCD_USBCMD_GLOBAL_RESET) ? TEXT("Set") : TEXT("Not Set"))));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tHost Controller Reset = %s\n"), ((dwData & UHCD_USBCMD_HOST_CONTROLLER_RESET) ? TEXT("Set") : TEXT("Not Set"))));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tRun/Stop = %s\n"), ((dwData & UHCD_USBCMD_RUN_STOP) ? TEXT("Run") : TEXT("Stop"))));
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tCHW - FAILED WHILE DUMPING USBCMD!!!\n")));
    }
}
// ******************************************************************
void CHW::DumpUSBSTS( void )
//
// Purpose: Queries Host Controller for contents of USBSTS, and prints
//          them to DEBUG output. Bit definitions are in UHCI spec 2.1.2
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: used in DEBUG mode only
//
//        This function is static
// ******************************************************************
{
    DWORD    dwData = 0;

    __try {
        dwData = Read_USBSTS();
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\tCHW - USB STATUS REGISTER (USBSTS) = 0x%X. Dump:\n"), dwData));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tHCHalted = %s\n"), ((dwData & UHCD_USBSTS_HCHALTED) ? TEXT("Halted") : TEXT("Not Halted"))));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tHost Controller Process Error = %s\n"), ((dwData & UHCD_USBSTS_HOST_CONTROLLER_PROCESS_ERROR) ? TEXT("Fatal Error") : TEXT("Not Set"))));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tHost System Error = %s\n"), ((dwData & UHCD_USBSTS_HOST_SYSTEM_ERROR) ? TEXT("Set") : TEXT("Not Set"))));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tResume Detect = %s\n"), ((dwData & UHCD_USBSTS_RESUME_DETECT) ? TEXT("Set") : TEXT("Not Set"))));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tUSB Error Interrupt = %s\n"), ((dwData & UHCD_USBSTS_USB_ERROR_INTERRUPT) ? TEXT("Set") : TEXT("Not Set"))));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tUSB Interrupt = %s\n"), ((dwData & UHCD_USBSTS_USB_INTERRUPT) ? TEXT("Set") : TEXT("Not Set"))));

    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tCHW - FAILED WHILE DUMPING USBSTS!!!\n")));
    }
}
// ******************************************************************
void CHW::DumpUSBINTR( void )
//
// Purpose: Queries Host Controller for contents of USBINTR, and prints
//          them to DEBUG output. Bit definitions are in UHCI spec 2.1.3
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: used in DEBUG mode only
//
//        This function is static
// ******************************************************************
{
    DWORD    dwData = 0;

    __try {
        dwData = Read_USBINTR();
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\tCHW - USB INTERRUPT REGISTER (USBINTR) = 0x%X. Dump:\n"), dwData));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tShort Packet Interrupt Enable = %s\n"), ((dwData & UHCD_USBINTR_SHORT_PACKET_INTERRUPT) ? TEXT("Enabled") : TEXT("Disabled"))));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tInterrupt on Complete (IOC) Enable = %s\n"), ((dwData & UHCD_USBINTR_INTERRUPT_ON_COMPLETE) ? TEXT("Enabled") : TEXT("Disabled"))));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tResume Interrupt Enable = %s\n"), ((dwData & UHCD_USBINTR_RESUME_INTERRUPT) ? TEXT("Enabled") : TEXT("Disabled"))));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tTimeout/CRC Interrupt Enable = %s\n"), ((dwData & UHCD_USBINTR_TIMEOUT_CRC_INTERRUPT) ? TEXT("Enabled") : TEXT("Disabled"))));
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tCHW - FAILED WHILE DUMPING USBINTR!!!\n")));
    }
}
// ******************************************************************
void CHW::DumpFRNUM( void )
//
// Purpose: Queries Host Controller for contents of FRNUM, and prints
//          them to DEBUG output. Bit definitions are in UHCI spec 2.1.4
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: used in DEBUG mode only
//
//        This function is static
// ******************************************************************
{
    DWORD    dwData = 0;

    __try {
        dwData = Read_FRNUM();
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\tCHW - FRAME NUMBER REGISTER (FRNUM) = 0x%X. Dump:\n"), dwData));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tFrame number (bits 10:0) = %d\n"), (dwData & UHCD_FRNUM_MASK)));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tFrame index (bits 9:0) = %d\n"), (dwData & UHCD_FRNUM_INDEX_MASK)));
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tCHW - FAILED WHILE DUMPING FRNUM!!!\n")));
    }
}

// ******************************************************************
void CHW::DumpFLBASEADD( void )
//
// Purpose: Queries Host Controller for contents of FLBASEADD, and prints
//          them to DEBUG output. Bit definitions are in UHCI spec 2.1.5
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: used in DEBUG mode only
//
//        This function is static
// ******************************************************************
{
    DWORD    dwData = 0;

    __try {
        dwData = Read_FLBASEADD();
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\tCHW - FRAME LIST BASE ADDRESS REGISTER (FLBASEADD) = 0x%X. Dump:\n"), dwData));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tFLBASEADD address base (bits 11:0 masked) = 0x%X\n"), (dwData & UHCD_FLBASEADD_MASK)));
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tCHW - FAILED WHILE DUMPING FLBASEADD!!!\n")));
    }
}
// ******************************************************************
void CHW::DumpSOFMOD( void )
//
// Purpose: Queries Host Controller for contents of SOFMOD, and prints
//          them to DEBUG output. Bit definitions are in UHCI spec 2.1.6
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: used in DEBUG mode only
//
//        This function is static
// ******************************************************************
{
    DWORD    dwData = 0;

    __try {
        dwData = Read_SOFMOD();
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\tCHW - START OF FRAME MODIFY REGISTER (SOFMOD) = 0x%X. Dump:\n"), dwData));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tFrame Length in 12Mhz clocks = %d\n"), (dwData & UHCD_SOFMOD_MASK) + UHCD_SOFMOD_MINIMUM_LENGTH));
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tCHW - FAILED WHILE DUMPING SOFMOD!!!\n")));
    }
}

// ******************************************************************
void CHW::DumpPORTSC(IN const USHORT port)
//
// Purpose: Queries Host Controller for contents of PORTSC #port, and prints
//          them to DEBUG output. Bit definitions are in UHCI spec 2.1.7
//
// Parameters: port - the port number to read. It must be such that
//                    1 <= port <= HCD_NUM_ROOT_HUB_PORTS
//
// Returns: Nothing
//
// Notes: used in DEBUG mode only
//
//        This function is static
// ******************************************************************
{
    DWORD    dwData = 0;

    __try {
        DEBUGCHK( port >=  1 && port <= HCD_NUM_ROOT_HUB_PORTS );

        dwData = Read_PORTSC( port );
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\tCHW - PORT STATUS AND CONTROL REGISTER (PORTSC%d) = 0x%X. Dump:\n"), port, dwData));
        if ( dwData & UHCD_PORTSC_PORT_ENABLED ) {
            DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tHub State = %s\n"), ((dwData & UHCD_PORTSC_SUSPEND) ? TEXT("Suspend") : TEXT("Enable"))));
        } else {
            DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tHub State = Disabled\n")));
        }
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tPort Reset = %s\n"), ((dwData & UHCD_PORTSC_PORT_RESET) ? TEXT("Reset") : TEXT("Not Reset"))));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tDevice Speed = %s\n"), ((dwData & UHCD_PORTSC_LOW_SPEED_DEVICE) ? TEXT("Low") : TEXT("Full"))));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tResume Detect = %s\n"), ((dwData & UHCD_PORTSC_RESUME_DETECT) ? TEXT("Set") : TEXT("Not Set"))));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tLine Status = %d\n"), ( (dwData & UHCD_PORTSC_LINE_STATUS) >> 4)));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tPort Enable/Disable Change  = %s\n"), ((dwData & UHCD_PORTSC_PORT_ENABLE_CHANGE) ? TEXT("Set") : TEXT("Not Set"))));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tPort = %s\n"), ((dwData & UHCD_PORTSC_PORT_ENABLED) ? TEXT("Enabled") : TEXT("Disabled"))));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tConnect Status Change = %s\n"), ((dwData & UHCD_PORTSC_CONNECT_STATUS_CHANGE) ? TEXT("Set") : TEXT("Not Set"))));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tConnect Status = %s\n"), ((dwData & UHCD_PORTSC_CONNECT_STATUS) ? TEXT("Device Present") : TEXT("No Device Present"))));

    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tCHW - FAILED WHILE DUMPING PORTSC%d!!!\n"), port));
    }
}

// ******************************************************************
void CHW::DumpAllRegisters( void )
//
// Purpose: Queries Host Controller for all registers, and prints
//          them to DEBUG output. Register definitions are in UHCI spec 2.1
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: used in DEBUG mode only
//
//        This function is static
// ******************************************************************
{
    DEBUGMSG(ZONE_REGISTERS, (TEXT("CHW - DUMP REGISTERS BEGIN\n")));
    DumpUSBCMD();
    DumpUSBSTS();
    DumpUSBINTR();
    DumpFRNUM();
    DumpFLBASEADD();
    DumpSOFMOD();
    for ( USHORT port = 1; port <= HCD_NUM_ROOT_HUB_PORTS; port++ ) {
        DumpPORTSC( port );
    }
    DEBUGMSG(ZONE_REGISTERS, (TEXT("CHW - DUMP REGISTERS DONE\n")));
}
#endif
