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
//     CPipe.cpp
// Abstract:  
//     Implements the Pipe class for managing open pipes for UHCI
//
//                             CPipe (ADT)
//                           /             \
//                  CQueuedPipe (ADT)       CIsochronousPipe
//                /         |       \ 
//              /           |         \
//   CControlPipe    CInterruptPipe    CBulkPipe
// 
// 
// Notes: 
// 
//

#include "cpipe.hpp"
#include "cphysmem.hpp"
#include "chw.hpp"
#include "cuhcd.hpp"

#ifndef _PREFAST_
#pragma warning(disable: 4068) // Disable pragma warnings
#endif

// ******************************************************************               
void InitializeTD( OUT PUHCD_TD const pTD,
                          IN const TD_LINK_POINTER_PHYSICAL_ADDRESS HW_paLink,
                          IN const PUHCD_TD vaNextTD,
                          IN const UCHAR InterruptOnComplete,
                          IN const UCHAR Isochronous,
                          IN const BOOL  LowSpeedControl,
                          IN const DWORD PID,
                          IN const UCHAR Address,
                          IN const UCHAR Endpoint,
                          IN const USHORT DataToggle,
                          IN const DWORD MaxLength,
                          IN const TD_BUFFER_PHYSICAL_ADDRESS HW_paBuffer, 
                          IN const BOOL bShortPacketOk /*= FALSE*/)
//
// Purpose: Fill in Transfer Descriptor fields
//
// Parameters: pTD - pointer to transfer descriptor to fill in
//
//             Rest of Params - various transfer descriptor fields
//
// Returns: Nothing
//
// Notes: MaxLength field should already be encoded by caller into 
//        (n-1) form
// ******************************************************************
{
    // HW DWORD 1 - Hardware link to the next item in schedule
    pTD->HW_paLink = HW_paLink;

    // HW DWORD 2
    // pTD->ActualLength <- don't need to set
    // pTD->Reserved_1 <- don't need to set
    pTD->Active = 1;
    pTD->StatusField = TD_STATUS_NO_ERROR;
    DEBUGCHK( (InterruptOnComplete & 1) == InterruptOnComplete );
    pTD->InterruptOnComplete = InterruptOnComplete;
    DEBUGCHK( (Isochronous & 1) == Isochronous );
    pTD->Isochronous = Isochronous;
    DEBUGCHK( (LowSpeedControl & 1) == LowSpeedControl );
    pTD->LowSpeedControl = LowSpeedControl;
    pTD->ErrorCounter = TD_ERRORCOUNTER_INTERRUPT_AFTER_THREE;
    pTD->ShortPacketDetect = bShortPacketOk ? 1 : 0;
    // pTD->ReservedMBZ <- don't need to set

    // HW DWORD 3
    DEBUGCHK( PID == TD_IN_PID ||
              PID == TD_OUT_PID ||
              PID == TD_SETUP_PID );
    pTD->PID = PID;
    DEBUGCHK( Address <= USB_MAX_ADDRESS );
    pTD->Address = Address;
    DEBUGCHK( (Endpoint & TD_ENDPOINT_MASK) == Endpoint );
    pTD->Endpoint = Endpoint;
    DEBUGCHK( (DataToggle & 1) == DataToggle );
    pTD->DataToggle = DataToggle;
    // pTD->Reserved_2 <- don't need to set
    // MaxLength is a nonzero length minus one or, for a zero length TD,
    // it might be seen here as a 32-bit -1 or as a zero-extended 11-bit -1.
    DEBUGCHK( (MaxLength == TD_MAXLENGTH_NULL_BUFFER) ||
              (MaxLength == (DWORD)-1) ||
              (MaxLength <= TD_MAXLENGTH_MAX && HW_paBuffer != 0) );
    pTD->MaxLength = MaxLength;

    // HW DWORD 4 - physical address of buffer
    pTD->HW_paBuffer = HW_paBuffer;

    // SW DWORD 5 - virt addr of previous Isoch TD
    pTD->vaPrevIsochTD = NULL;

    // SW DWORD 6 - virt addr of next TD in list
    pTD->vaNextTD = vaNextTD;

#ifdef DEBUG
    // SW DWORD 7 - Unused for now
    pTD->dwUNUSED1 = 0xdeadbeef;
    // SW DWORD 8 - Unused for now
    pTD->dwUNUSED2 = 0xdeadbeef;
#endif // DEBUG
}


// ******************************************************************               
void InitializeQH( OUT PUHCD_QH const pQH, 
                          IN const PUHCD_QH vaPrevQH,
                          IN const QUEUE_HEAD_LINK_POINTER_PHYSICAL_ADDRESS HW_paHLink,
                          IN const PUHCD_QH vaNextQH )
//
// Purpose: Fill in Queue Head Fields
//
// Parameters: pQH - pointer to Queue Head to Initialize
//
//             Rest of Params - various Queue Head fields
//
// Returns: Nothing
//
// Notes: 
// ******************************************************************
{
    PREFAST_DEBUGCHK( pQH != NULL );

    // HW DWORD #1 - Horizontal link 
    pQH->HW_paHLink = HW_paHLink;
    // HW DWORD #2 - Vertical link - No Transfer Descriptors right now
    pQH->HW_paVLink = QUEUE_ELEMENT_LINK_POINTER_TERMINATE;
    // SW DWORD #3 - previous QH in schedule, or NULL
    pQH->vaPrevQH = vaPrevQH;
    // SW DWORD #4 - next QH in schedule, or NULL
    pQH->vaNextQH = vaNextQH;
    // SW DWORD #5 - first Transfer Descriptor - NULL for now
    pQH->vaVertTD = NULL;
#ifdef DEBUG
    // SW DWORD #6
    pQH->dwInterruptTree.Load = 0xdeadbeef;
    // SW DWORD #7
    pQH->dwUNUSED1 = 0xdeadbeef;
    // SW DWORD #8
    pQH->dwUNUSED2 = 0xdeadbeef;
#endif // DEBUG
}

CUHCIFrame::CUHCIFrame(IN CPhysMem* const pCPhysMem ) 
:   m_pCPhysMem( pCPhysMem)
{
// memory manager
    numReclamationTransfers = 0;
    m_pCUhcd=NULL;
#ifdef DEBUG
    m_debug_fInitializeAlreadyCalled = FALSE;
#endif // DEBUG

// schedule related vars
    m_vaFrameList = NULL;
    m_pFinalQH = NULL;
    m_pFrameSynchTD = NULL;
                                    
#ifdef DEBUG
    m_debug_TDMemoryAllocated = 0;
    m_debug_QHMemoryAllocated = 0;
    m_debug_BufferMemoryAllocated = 0;
    m_debug_ControlExtraMemoryAllocated = 0;
#endif // DEBUG

// Handle Done Transfers thread variables
    m_fCheckTransferThreadClosing = FALSE;
    m_hCheckForDoneTransfersEvent = NULL;
    m_hCheckForDoneTransfersThread = NULL;
    m_pBusyPipeList = NULL;
#ifdef DEBUG
    m_debug_numItemsOnBusyPipeList = 0;
#endif // DEBUG
};
CUHCIFrame::~CUHCIFrame()
{
    DeInitialize();
}
// *****************************************************************
// Scope: public static
BOOL CUHCIFrame::Initialize(  CUhcd * pCUhcd)
//
// Purpose: Initialize CPipe's static variables. This
//          also sets up the original empty schedule
//          with the frame list, and interrupt Queue Head tree.
//          We also set up a thread for processing done transfers
//
// Parameters: pCPhysMem - pointer to memory manager object
//
// Returns: TRUE - if everything initialized ok
//          FALSE - in case of failure
//
// Notes: This function is only called from the CUhcd::Initialize routine.
//        It should only be called once, to initialize the static variables
// ******************************************************************
{
    DEBUGMSG(ZONE_INIT, (TEXT("+CUHCIFrame::Initialize\n")));
    m_pCUhcd=pCUhcd;
    
#ifdef DEBUG // only call this once to init static vars/schedule
    DEBUGCHK( m_debug_fInitializeAlreadyCalled == FALSE );
    m_debug_fInitializeAlreadyCalled = TRUE;
#endif // DEBUG

    // TDs and QHs must satisfy certain size/alignment criteria
    DEBUGCHK( sizeof( UHCD_TD ) == TD_REQUIRED_SIZE_IN_BYTES );
    DEBUGCHK( sizeof( UHCD_QH ) % QH_ALIGNMENT_BOUNDARY == 0 );
    // memory manager will provide alignment - check if it
    // will satisfy our TD/QH requirements
    DEBUGCHK( CPHYSMEM_MEMORY_ALIGNMENT % TD_ALIGNMENT_BOUNDARY == 0 );
    DEBUGCHK( CPHYSMEM_MEMORY_ALIGNMENT % QH_ALIGNMENT_BOUNDARY == 0 );

    // init static variables
    // frame list variables
    InitializeCriticalSection( &m_csFrameListLock );
    DEBUGCHK( m_vaFrameList == NULL );
    // QH scheduling variables
    InitializeCriticalSection( &m_csQHScheduleLock );
    memset( m_interruptQHTree, 0, 2 * UHCD_MAX_INTERRUPT_INTERVAL * sizeof( PUHCD_QH ) );
    // check for done transfers thread variables
    DEBUGCHK( m_fCheckTransferThreadClosing == FALSE &&
              m_hCheckForDoneTransfersThread == NULL &&
              m_hCheckForDoneTransfersEvent == NULL );
    DEBUGCHK( m_pBusyPipeList == NULL &&
              m_debug_numItemsOnBusyPipeList == 0 );
    InitializeCriticalSection( &m_csBusyPipeListLock );

    if ( m_pCPhysMem == NULL ) {
        DEBUGCHK( 0 ); // This should never happen
        DEBUGMSG(ZONE_ERROR, (TEXT("-CPipe::Initialize, no memory manager object!!\n")));
        return FALSE;
    }

    // m_hCheckForDoneTransfersEvent - Auto Reset, and Initial State = non-signaled
    m_hCheckForDoneTransfersEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
    if ( m_hCheckForDoneTransfersEvent == NULL ) {
        DEBUGMSG(ZONE_ERROR, (TEXT("-CPipe::Initialize. Error creating process done transfers event\n")));
        return FALSE;
    }

    // set up our thread to check for done transfers
    // currently, the context passed to CheckForDoneTransfersThread is ignored
    m_hCheckForDoneTransfersThread = CreateThread( 0, 0, CheckForDoneTransfersThreadStub, (PVOID)this, 0, NULL );
    if ( m_hCheckForDoneTransfersThread == NULL ) {
        DEBUGMSG(ZONE_ERROR, (TEXT("-CPipe::Initialize. Error creating process done transfers thread\n")));
        return FALSE;
    }
    CeSetThreadPriority( m_hCheckForDoneTransfersThread, g_IstThreadPriority + RELATIVE_PRIO_CHECKDONE );

    // ask CHW about the frame list
    {
        // 
        // The frame list is just an array of FRAME_LIST_LENGTH pointers. Each
        // is either invalid, or points to a TD/QH which starts the schedule for
        // that frame.
        //
        // frame list should be 4Kb according to UHCI spec
        DEBUGCHK( FRAME_LIST_SIZE_IN_BYTES == 4096 );
        // frame list should be same size as USBPAGESIZE used by memory manager
        DEBUGCHK( FRAME_LIST_SIZE_IN_BYTES == USBPAGESIZE );
    
        DEBUGCHK( m_vaFrameList == NULL );
        m_vaFrameList = m_pCUhcd->CHW::GetFrameListAddr(); // was allocated from CPhysMem by CHW
    
        // the frame list MUST be aligned on a 4Kb memory boundary
        if ( GetFrameListPhysAddr() % FRAME_LIST_SIZE_IN_BYTES != 0 ) {
            DEBUGMSG(ZONE_ERROR, (TEXT("-CPipe::Initialize - Error. Unable to create frame list. m_vaFrameList = 0x%X\n"), m_vaFrameList ));
            DEBUGCHK( 0 ); // this should never happen
            return FALSE;
        }
    
        DEBUGCHK( m_vaFrameList != NULL );
        DEBUGMSG(ZONE_INIT && ZONE_VERBOSE, (TEXT("CPipe::Initialize - Created frame list. m_vaFrameList = 0x%X, PhysAddr=0x%X\n"), m_vaFrameList, GetFrameListPhysAddr() ));
    } 

    // set up the queue head scheduling structures
    {
        // We set this up as follows:
        // (demonstrated for UHCD_MAX_INTERRUPT_INTERVAL = 4)
        // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
        //
        //                         level 2 level 1 level 0    Full Speed QH     End of Schedule 
        //
        //   frame #0 ->(isoch)-> QH #4  \
        //                                  QH #2
        //   frame #2 ->(isoch)-> QH #6  /      \                 
        //                                               (low speed)     (full speed)
        //                                          QH # 1 - - - - QH #0 - - - - - - FinalQH
        //                                        /                  ^                 |
        //   frame #1 ->(isoch)-> QH #5  \                           |                 |
        //                                  QH #3                    -------------------
        //   frame #3 ->(isoch)-> QH #7  /                             (Reclamation)
        //
        //   etc for rest of frames
        //
        //   The outermost QHs will be numbered i = UHCD_MAX_INTERRUPT_INTERVAL + x, where
        //   x = 0, 1, 2, 3, .., UHCD_MAX_INTERRUPT_INTERVAL - 1. The QH #i will be 
        //   scheduled by any frame which has index == x (mod UHCD_MAX_INTERRUPT_INTERVAL)
        //
        //   Note that the QHs in the kth level will be executed every
        //   2^k frames. Take any number n. If 2^k <= n < 2^(k+1), then
        //   QH #n is placed in the tree at level k. When we want to schedule
        //   a new queue every 2^k frames, we can place it after any existing
        //   tree queues 2^k, 2^k + 1, 2^k + 2, ..., 2^(k+1) - 1
        //
        //   Given QH #n (n > 1), if n in binary is 1x(remaining_bits)b,
        //   then QH #n links to QH # 01(remaining_bits)b.
        //   For instance, 7 = 111b links to 3 = 011b
        //   and also      4 = 100b links to 2 = 010b
        //
        //   ISOCHRONOUS TDs will be placed in between the frame list and the first level
        //   of the interrupt QH tree. 
        //
        //   INTERRUPT transfers will be placed at the appropriate point within the QH
        //   tree so that they are scheduled at regular intervals.
        //
        //   CONTROL transfers go directly after QH #1, if they are low speed, or if they
        //   have max packet size > MAX_RECLAMATION_PACKET_SIZE. They go after QH #0 if they
        //   are full speed and have max packet size <= MAX_RECLAMATION_PACKET_SIZE.
        //
        //   BULK transfers are always high speed. If the max packet size is less than
        //   MAX_RECLAMATION_PACKET_SIZE, the transfer goes before FinalQH, else it
        //   goes *before* QH # 0 

        // we need 2 * UHCD_MAX_INTERRUPT_INTERVAL + 1 queue heads which
        // will always remain in the schedule and never be freed
        PUCHAR vaQHList = NULL;
#define CPIPE_INITIALIZE_QH_MEMORY_NEEDED DWORD( (2 * UHCD_MAX_INTERRUPT_INTERVAL + 1) * sizeof( UHCD_QH ) )
        if ( !m_pCPhysMem->AllocateMemory( DEBUG_PARAM( TEXT("Permanent QHs") )
                                           CPIPE_INITIALIZE_QH_MEMORY_NEEDED,
                                           &vaQHList,
                                           CPHYSMEM_FLAG_NOBLOCK ) ) {
            DEBUGMSG(ZONE_ERROR, (TEXT("-CPipe::Initialize - Could not get QHs for schedule\n")));
            return FALSE;
        }
    #ifdef DEBUG
        m_debug_QHMemoryAllocated += CPIPE_INITIALIZE_QH_MEMORY_NEEDED;
        DEBUGMSG( ZONE_QH, (TEXT("CPipe::Initialize - allocate persistant QHs, total bytes = %d\n"), m_debug_QHMemoryAllocated) );
    #endif // DEBUG
        m_pFinalQH = (PUHCD_QH) vaQHList;
        vaQHList += sizeof( UHCD_QH );
        m_interruptQHTree[ 0 ] = (PUHCD_QH) vaQHList;
        vaQHList += sizeof( UHCD_QH );
        // m_pFinalQH is at the very end of the schedule, and points back
        // to QH # 0. For now, the terminate bit is set in HLink, because
        // there are no high speed bulk/control transfers scheduled
        InitializeQH( m_pFinalQH, // QH to initialize
                      m_interruptQHTree[ 0 ], // previous QH
                      GetQHPhysAddr( m_interruptQHTree[ 0 ] )
                             | QUEUE_HEAD_LINK_POINTER_QH
                             | QUEUE_HEAD_LINK_POINTER_TERMINATE, // HW link to next QH
                      m_interruptQHTree[ 0 ] ); // next QH

        // QH # 0 points to m_pFinalQH
        InitializeQH( m_interruptQHTree[ 0 ], // QH to initialize
                      NULL, // previous QH - we'll set this below
                      GetQHPhysAddr( m_pFinalQH )
                            | QUEUE_HEAD_LINK_POINTER_QH
                            | QUEUE_HEAD_LINK_POINTER_VALID, // HW link to next QH
                      m_pFinalQH ); // next QH

#ifndef UHCD_DONT_ALLOW_RECLAMATION_FOR_CONTROL_PIPES
        //
        // Implement reclamation workaround from Intel PIIX4E specification update.
        //
        if ( !m_pCPhysMem->AllocateMemory( DEBUG_PARAM( TEXT("QH#0 Pseudo TD") )
                                           sizeof( UHCD_TD ),
                                           (PUCHAR *) &m_pFinalQH->vaVertTD,
                                           CPHYSMEM_FLAG_NOBLOCK ) ) {
            DEBUGMSG( ZONE_ERROR, (TEXT("-CPipe::Initialize - no memory for QH#0 pseudo TD\n")) );
            return FALSE;
        }
#ifdef DEBUG
        m_debug_TDMemoryAllocated += sizeof( UHCD_TD );
        DEBUGMSG( ZONE_TD, (TEXT("CPipe::Initialize - alloc 1 TD, total bytes = %d\n"), m_debug_TDMemoryAllocated ) );
#endif // DEBUG

        InitializeTD( m_pFinalQH->vaVertTD,
                      GetTDPhysAddr( m_pFinalQH->vaVertTD )
                             | TD_LINK_POINTER_TD
                             | TD_LINK_POINTER_VALID
                             | TD_LINK_POINTER_BREADTH_FIRST,
                      NULL,                // vaNextTD (use NULL so destructor doesn't have to detect loops)
                      FALSE, FALSE, FALSE,          // fIOC, fIsoch, fLowSpd
                      TD_OUT_PID, 0, 0, 0,          // pid, addr, ep, dataTog
                      TD_MAXLENGTH_NULL_BUFFER, 0   // maxLen, HW_paBuffer (this TD will always be inactive)
                    );
        m_pFinalQH->vaVertTD->Active = 0;

        m_pFinalQH->HW_paVLink = GetTDPhysAddr( m_pFinalQH->vaVertTD )
                                        | QUEUE_ELEMENT_LINK_POINTER_TD
                                        | QUEUE_ELEMENT_LINK_POINTER_VALID;
#endif // UHCD_DONT_ALLOW_RECLAMATION_FOR_CONTROL_PIPES

        for ( UCHAR pow = 1; pow <= UHCD_MAX_INTERRUPT_INTERVAL; pow <<= 1 ) {
            for ( UCHAR index = pow; index < (pow << 1); index++ ) {
                DEBUGCHK( m_interruptQHTree[ index ] == NULL );
                m_interruptQHTree[ index ] = PUHCD_QH( vaQHList );
                vaQHList += sizeof( UHCD_QH );
                const UCHAR link = (index ^ pow) | (pow >> 1);
                DEBUGCHK( m_interruptQHTree[ link ] != NULL );
                // IMPORTANT!!! Previous fields are not set in the interruptQHTree
                // structure, since there are 2 previous QHs. Instead, we use
                // the fact that previous QHs can be calculated by index number.
                // The previous field of one of the interruptQHtree members is not
                // guaranteed to be valid anywhere.
                InitializeQH( m_interruptQHTree[ index ], // QH to initialize
                              NULL, // previous QH not set for interrupt tree
                              GetQHPhysAddr( m_interruptQHTree[ link ] )
                                    | QUEUE_HEAD_LINK_POINTER_QH 
                                    | QUEUE_HEAD_LINK_POINTER_VALID, // HW link to next QH
                              m_interruptQHTree[ link ] ); // next QH

                // there are originally no scheduled interrupts
                m_interruptQHTree[ index ]->dwInterruptTree.Load = 0;
            }
        }
        DEBUGCHK( m_interruptQHTree[ 0 ]->vaPrevQH == NULL );
        m_interruptQHTree[ 0 ]->vaPrevQH = m_interruptQHTree[ 1 ];
    }

    // now set up the frame list
    for ( DWORD frame = 0; frame < FRAME_LIST_LENGTH; frame++ ) {
        DEBUGCHK( sizeof( m_vaFrameList[ frame ] ) == sizeof( FRAME_LIST_POINTER ) );

        m_vaFrameList[ frame ] = GetQHPhysAddr( m_interruptQHTree[ QHTreeEntry( frame ) ] )
                                     | FRAME_LIST_POINTER_QH
                                     | FRAME_LIST_POINTER_VALID;
    }                                                 
    // we need one persistant inactive Isoch TD to guarantee that
    // we are interrupted at least once every FRAME_LIST_LENGTH frames.
    // Otherwise, our internal frame counter in CHW will miss frames.
    {
#define CPIPE_INITIALIZE_TD_MEMORY_NEEDED  sizeof( UHCD_TD )
        if ( !m_pCPhysMem->AllocateMemory(
                                    DEBUG_PARAM( TEXT("Permanent Isoch TD") )
                                    CPIPE_INITIALIZE_TD_MEMORY_NEEDED,
                                    (PUCHAR*)&m_pFrameSynchTD,
                                    CPHYSMEM_FLAG_NOBLOCK ) ) {
            DEBUGMSG( ZONE_ERROR, (TEXT("-CPipe::Initialize - no memory for Frame Synch Isoch TD\n")) );
            return FALSE;
        }
    #ifdef DEBUG
        m_debug_TDMemoryAllocated += CPIPE_INITIALIZE_TD_MEMORY_NEEDED;
        DEBUGMSG( ZONE_TD, (TEXT("CPipe::Initialize - create permanent TDs, total bytes = %d\n"), m_debug_TDMemoryAllocated) );
    #endif // DEBUG
        InitializeTD( m_pFrameSynchTD,
                      m_vaFrameList[ 0 ], // HW_paLink
                      NULL, // vaNextTD
                      1, // InterruptOnComplete
                      1, // Isochronous
                      0, // Low Speed Control (not used)
                      TD_OUT_PID, // PID (must be valid, even though not used)
                      USB_MAX_ADDRESS, // Device Address (not used)
                      0, // Device Endpoint (not used)
                      0, // Data Toggle (not used)
                      TD_MAXLENGTH_NULL_BUFFER, // no data buffer
                      0 ); // physical address of data buffer
        // Active is set to 1 by InitializeTD
        DEBUGCHK( m_pFrameSynchTD->Active == 1 );
        m_pFrameSynchTD->Active = 0;

        // insert into schedule
        m_vaFrameList[ 0 ] = GetTDPhysAddr( m_pFrameSynchTD ) 
                                    | FRAME_LIST_POINTER_TD
                                    | FRAME_LIST_POINTER_VALID;
    }

    DEBUGMSG(ZONE_INIT, (TEXT("-CUHCIFrame::Initialize. Success!\n")));
    return TRUE;
}

// *****************************************************************
// Scope: public static
void CUHCIFrame::DeInitialize( void )
//
// Purpose: DeInitialize any static variables
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: This function should only be called from the ~CUhcd()
// ******************************************************************
{
#ifdef DEBUG // don't call this twice
    DEBUGCHK( m_debug_fInitializeAlreadyCalled );
    m_debug_fInitializeAlreadyCalled = FALSE;
#endif // DEBUG

    m_fCheckTransferThreadClosing = TRUE;
    // Wake up the CheckForDoneTransfersThread and give it time to die
    if ( m_hCheckForDoneTransfersEvent ) {
        SetEvent( m_hCheckForDoneTransfersEvent );
        if ( m_hCheckForDoneTransfersThread ) {
            DWORD dwWaitReturn = WaitForSingleObject( m_hCheckForDoneTransfersThread, 5000 );
            if ( dwWaitReturn != WAIT_OBJECT_0 ) {
                DEBUGCHK( 0 ); // check why thread is blocked
#pragma prefast(suppress: 258, "Try to recover gracefully from a pathological failure")
                TerminateThread( m_hCheckForDoneTransfersThread, DWORD(-1) );
            }
            CloseHandle( m_hCheckForDoneTransfersThread );
            m_hCheckForDoneTransfersThread = NULL;
        }
        CloseHandle( m_hCheckForDoneTransfersEvent );
        m_hCheckForDoneTransfersEvent = NULL;
    }

    // all busy pipes should have been closed by now
    DEBUGCHK( m_pBusyPipeList == NULL &&
              m_debug_numItemsOnBusyPipeList == 0 );
    DeleteCriticalSection( &m_csBusyPipeListLock );

    DeleteCriticalSection( &m_csFrameListLock );
    DeleteCriticalSection( &m_csQHScheduleLock );

    m_fCheckTransferThreadClosing = FALSE;

#ifndef UHCD_DONT_ALLOW_RECLAMATION_FOR_CONTROL_PIPES
    m_pCPhysMem->FreeMemory( PUCHAR(m_pFinalQH->vaVertTD),
                             GetTDPhysAddr( m_pFinalQH->vaVertTD ),
                             CPHYSMEM_FLAG_NOBLOCK );
#endif

    m_pCPhysMem->FreeMemory( PUCHAR(m_pFinalQH),
                             GetQHPhysAddr( m_pFinalQH ),
                             CPHYSMEM_FLAG_NOBLOCK );
    m_pFinalQH = NULL;

    m_pCPhysMem->FreeMemory( PUCHAR(m_pFrameSynchTD),
                             GetTDPhysAddr( m_pFrameSynchTD ),
                             CPHYSMEM_FLAG_NOBLOCK );

    m_vaFrameList = NULL;
}


// ******************************************************************
// Scope: public static
void CUHCIFrame::SignalCheckForDoneTransfers( void )
//
// Purpose: This function is called when an interrupt is received by
//          the CHW class. We then signal the CheckForDoneTransfersThread
//          to check for any transfers which have completed
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: DO MINIMAL WORK HERE!! Most processing should be handled by
//        The CheckForDoneTransfersThread. If this procedure blocks, it
//        will adversely affect the interrupt processing thread.
// ******************************************************************
{
    DEBUGCHK( m_hCheckForDoneTransfersEvent && m_hCheckForDoneTransfersThread );
    SetEvent( m_hCheckForDoneTransfersEvent );
}
// ******************************************************************
ULONG CALLBACK CUHCIFrame::CheckForDoneTransfersThreadStub( IN PVOID pContext)
{
    return ((CUHCIFrame *)pContext)->CheckForDoneTransfersThread( );
}
// Scope: private static
ULONG CUHCIFrame::CheckForDoneTransfersThread( )
//
// Purpose: Thread for checking whether busy pipes are done their
//          transfers. This thread should be activated whenever we
//          get a USB transfer complete interrupt (this can be
//          requested by the InterruptOnComplete field of the TD)
//
// Parameters: 32 bit pointer passed when instantiating thread (ignored)
//                       
// Returns: 0 on thread exit
//
// Notes: 
// ******************************************************************
{
    SetKMode(TRUE);

    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("+CPipe::CheckForDoneTransfersThread\n")) );

    PPIPE_LIST_ELEMENT pPrev = NULL;
    PPIPE_LIST_ELEMENT pCurrent = NULL;

    DEBUGCHK( m_hCheckForDoneTransfersEvent != NULL );

    while ( !m_fCheckTransferThreadClosing ) {
        WaitForSingleObject( m_hCheckForDoneTransfersEvent, INFINITE );
        if ( m_fCheckTransferThreadClosing ) {
            break;
        }

        EnterCriticalSection( &m_csBusyPipeListLock );
    #ifdef DEBUG // make sure m_debug_numItemsOnBusyPipeList is accurate
        {
            int debugCount = 0;
            PPIPE_LIST_ELEMENT pDebugElement = m_pBusyPipeList;
            while ( pDebugElement != NULL ) {
                pDebugElement = pDebugElement->pNext;
                debugCount++;
            }
            DEBUGCHK( debugCount == m_debug_numItemsOnBusyPipeList );
        }
        BOOL fDebugNeedProcessing = m_debug_numItemsOnBusyPipeList > 0;
        DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE && fDebugNeedProcessing, (TEXT("CPipe::CheckForDoneTransfersThread - #pipes to check = %d\n"), m_debug_numItemsOnBusyPipeList) );
        DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE && !fDebugNeedProcessing, (TEXT("CPipe::CheckForDoneTransfersThread - warning! Called when no pipes were busy\n")) );
    #endif // DEBUG
        pPrev = NULL;
        pCurrent = m_pBusyPipeList;
        while ( pCurrent != NULL ) {
            if ( pCurrent->pPipe->CheckForDoneTransfers() ) {
                // I was printing pPipe->GetPipeType() in the past
                // but, after this thread's priority was accidentally
                // set too low, it happened that the pipe was closed
                // at exactly this point. That is completely legitimate.
                // So, don't access pPipe->Anything past this point!!
                DEBUGMSG( ZONE_PIPE, (TEXT("CPipe::CheckForDoneTransfersThread - removing pipe 0x%x from busy list\n"), pCurrent->pPipe) );
            #ifdef DEBUG
                pCurrent->pPipe = NULL;
            #endif // DEBUG
                // this pipe is not busy any more. Remove it from the linked list,
                // and move to next item
                if ( pCurrent == m_pBusyPipeList ) {
                    DEBUGCHK( pPrev == NULL );
                    m_pBusyPipeList = m_pBusyPipeList->pNext;
                    delete pCurrent;
                    pCurrent = m_pBusyPipeList;
                } else {
                    DEBUGCHK( pPrev != NULL && pPrev->pNext == pCurrent );
                    pPrev->pNext = pCurrent->pNext;
                    delete pCurrent;
                    pCurrent = pPrev->pNext;
                }
                DEBUGCHK( --m_debug_numItemsOnBusyPipeList >= 0 );
            } else {
                // this pipe is still busy. Move to next item
                pPrev = pCurrent;
                pCurrent = pPrev->pNext;
            }
        }
        DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE && fDebugNeedProcessing, (TEXT("CPipe::CheckForDoneTransfersThread - #pipes still busy = %d\n"), m_debug_numItemsOnBusyPipeList) );
        LeaveCriticalSection( &m_csBusyPipeListLock );
    }
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("-CPipe::CheckForDoneTransfersThread\n")) );
    return 0;
}
// ******************************************************************
// Scope: protected static 
BOOL CUHCIFrame::AddToBusyPipeList( IN CPipe * const pPipe,
                               IN const BOOL fHighPriority )
//
// Purpose: Add the pipe indicated by pPipe to our list of busy pipes.
//          This allows us to check for completed transfers after 
//          getting an interrupt, and being signaled via 
//          SignalCheckForDoneTransfers
//
// Parameters: pPipe - pipe to add to busy list
//
//             fHighPriority - if TRUE, add pipe to start of busy list,
//                             else add pipe to end of list.
//
// Returns: TRUE if pPipe successfully added to list, else FALSE
//
// Notes: 
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE, (TEXT("+CPipe::AddToBusyPipeList - new pipe(%s) 0x%x, pri %d\n"), pPipe->GetPipeType(), pPipe, fHighPriority ));

    PREFAST_DEBUGCHK( pPipe != NULL );
    BOOL fSuccess = FALSE;

    // make sure there nothing on the pipe already (it only gets officially added after this function succeeds).
    DEBUGCHK( !pPipe->m_fTransferInProgress );

    EnterCriticalSection( &m_csBusyPipeListLock );
#ifdef DEBUG
{
    // make sure this pipe isn't already in the list. That should never happen.
    // also check that our m_debug_numItemsOnBusyPipeList is correct
    PPIPE_LIST_ELEMENT pBusy = m_pBusyPipeList;
    int count = 0;
    while ( pBusy != NULL ) {
        DEBUGCHK( pBusy->pPipe != NULL &&
                  pBusy->pPipe != pPipe );
        pBusy = pBusy->pNext;
        count++;
    }
    DEBUGCHK( m_debug_numItemsOnBusyPipeList == count );
}
#endif // DEBUG
    
    PPIPE_LIST_ELEMENT pNewBusyElement = new PIPE_LIST_ELEMENT;
    if ( pNewBusyElement != NULL ) {
        pNewBusyElement->pPipe = pPipe;
        if ( fHighPriority || m_pBusyPipeList == NULL ) {
            // add pipe to start of list
            pNewBusyElement->pNext = m_pBusyPipeList;
            m_pBusyPipeList = pNewBusyElement;
        } else {
            // add pipe to end of list
            PPIPE_LIST_ELEMENT pLastElement = m_pBusyPipeList;
            while ( pLastElement->pNext != NULL ) {
                pLastElement = pLastElement->pNext;
            }
            pNewBusyElement->pNext = NULL;
            pLastElement->pNext = pNewBusyElement;
        }
        fSuccess = TRUE;
    #ifdef DEBUG
        m_debug_numItemsOnBusyPipeList++;
    #endif // DEBUG
    }
    LeaveCriticalSection( &m_csBusyPipeListLock );

    DEBUGMSG( ZONE_PIPE, (TEXT("-CPipe::AddToBusyPipeList - new pipe(%s) 0x%x, pri %d, returning BOOL %d\n"), pPipe->GetPipeType(), pPipe, fHighPriority, fSuccess) );
    return fSuccess;
}

// ******************************************************************
// Scope: protected static
void CUHCIFrame::RemoveFromBusyPipeList( IN CPipe * const pPipe )
//
// Purpose: Remove this pipe from our busy pipe list. This happens if
//          the pipe is suddenly aborted or closed while a transfer
//          is in progress
//
// Parameters: pPipe - pipe to remove from busy list
//
// Returns: Nothing
//
// Notes: 
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("+CPipe::RemoveFromBusyPipeList - pipe(%s) 0x%x\n"), pPipe->GetPipeType(), pPipe ) );

    EnterCriticalSection( &m_csBusyPipeListLock );
#ifdef DEBUG
    BOOL debug_fRemovedPipe = FALSE;
{
    // check m_debug_numItemsOnBusyPipeList
    PPIPE_LIST_ELEMENT pBusy = m_pBusyPipeList;
    int count = 0;
    while ( pBusy != NULL ) {
        DEBUGCHK( pBusy->pPipe != NULL );
        pBusy = pBusy->pNext;
        count++;
    }
    DEBUGCHK( m_debug_numItemsOnBusyPipeList == count );
}
#endif // DEBUG
    PPIPE_LIST_ELEMENT pPrev = NULL;
    PPIPE_LIST_ELEMENT pCurrent = m_pBusyPipeList;
    while ( pCurrent != NULL ) {
        if ( pCurrent->pPipe == pPipe ) {
            // Remove item from the linked list
            if ( pCurrent == m_pBusyPipeList ) {
                DEBUGCHK( pPrev == NULL );
                m_pBusyPipeList = m_pBusyPipeList->pNext;
            } else {
                DEBUGCHK( pPrev != NULL &&
                          pPrev->pNext == pCurrent );
                pPrev->pNext = pCurrent->pNext;
            }
            delete pCurrent;
            pCurrent = NULL;
        #ifdef DEBUG
            debug_fRemovedPipe = TRUE;
            DEBUGCHK( --m_debug_numItemsOnBusyPipeList >= 0 );
        #endif // DEBUG
            break;
        } else {
            // Check next item
            pPrev = pCurrent;
            pCurrent = pPrev->pNext;
        }
    }
    LeaveCriticalSection( &m_csBusyPipeListLock );

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE && debug_fRemovedPipe, (TEXT("-CPipe::RemoveFromBusyPipeList, removed pipe(%s) 0x%x\n"), pPipe->GetPipeType(), pPipe));
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE && !debug_fRemovedPipe, (TEXT("-CPipe::RemoveFromBusyPipeList, pipe(%s) 0x%x was not on busy list\n"), pPipe->GetPipeType(), pPipe ));
}


// ******************************************************************               
// Scope: private static
void CUHCIFrame::InsertIsochTDIntoFrameList( IN_OUT PUHCD_TD pTD,
                                                   IN const DWORD dwFrameIndex )
//
// Purpose: Inserts an isochronous transfer descriptor (TD) into the schedule
//          
//
// Parameters: pTD - pointer to TD to insert into schedule. This
//                   TD will have some of its fields changed by this function.
//
//             dwFrameIndex - indicates frame index in which to schedule this TD.
//                            Caller should ensure 0 <= index < FRAME_LIST_LENGTH
//
// Returns: Nothing
//
// Notes: Assumes m_csFrameListLock is held
// ******************************************************************
{
    PREFAST_DEBUGCHK( pTD != NULL );
    DEBUGCHK( pTD->Isochronous == 1 );
    DEBUGCHK( dwFrameIndex >= 0 &&
              dwFrameIndex < FRAME_LIST_LENGTH );

    if ( m_vaFrameList[ dwFrameIndex ] & FRAME_LIST_POINTER_QH ) {
        // there are no isoch TDs presently scheduled in this frame
        DEBUGCHK( m_vaFrameList[ dwFrameIndex ] == ( GetQHPhysAddr( m_interruptQHTree[ QHTreeEntry( dwFrameIndex ) ] )
                                                                         | FRAME_LIST_POINTER_QH
                                                                         | FRAME_LIST_POINTER_VALID ) );
        pTD->vaNextTD = NULL;
    } else {
        // there are already isoch TDs scheduled in this frame
        PUHCD_TD vaNextIsochTD = (PUHCD_TD) m_pCPhysMem->PaToVa( m_vaFrameList[ dwFrameIndex ] & FRAME_LIST_POINTER_MASK );

        DEBUGCHK( m_vaFrameList[ dwFrameIndex ] == ( GetTDPhysAddr( vaNextIsochTD )
                                                         | FRAME_LIST_POINTER_TD
                                                         | FRAME_LIST_POINTER_VALID ) );
        DEBUGCHK( vaNextIsochTD->Isochronous == 1 &&
                  vaNextIsochTD->vaPrevIsochTD == NULL );

        pTD->vaNextTD = vaNextIsochTD;
        vaNextIsochTD->vaPrevIsochTD = pTD;
    }

    // now insert pTD after the frame list
    pTD->vaPrevIsochTD = NULL;
    pTD->HW_paLink = m_vaFrameList[ dwFrameIndex ];
    m_vaFrameList[ dwFrameIndex ] = GetTDPhysAddr( pTD )
                                        | FRAME_LIST_POINTER_TD
                                        | FRAME_LIST_POINTER_VALID;
}

// ******************************************************************               
// Scope: private static
void CUHCIFrame::RemoveIsochTDFromFrameList( IN_OUT PUHCD_TD pTD,
                                                   IN const DWORD dwFrameIndex )
//
// Purpose: Removes an isochronous transfer descriptor (TD) from the schedule
//          
//
// Parameters: pTD - pointer to TD to remove from schedule. This
//                   TD will have some of its fields changed by this function.
//
//             dwFrameIndex - frame index of this TD. Indicates which frame
//                            the TD is scheduled in. Caller must ensure that
//                            0 <= dwFrameIndex < FRAME_LIST_LENGTH
//
// Returns: Nothing
//
// Notes: Assumes m_csFrameListLock is held
//
//        The TD is not deleted or freed - it is just removed from the
//        schedule. The caller still needs to ensure that HW is not
//        accessing the TD before it can be freed.
// ******************************************************************
{

    PREFAST_DEBUGCHK( pTD != NULL );
    DEBUGCHK( pTD->Isochronous == 1 );
    DEBUGCHK( dwFrameIndex >= 0 &&
              dwFrameIndex < FRAME_LIST_LENGTH );
#ifdef DEBUG
{
    // first make sure that this TD is in the given frame
    DEBUGCHK( !(m_vaFrameList[ dwFrameIndex ] & FRAME_LIST_POINTER_QH) );
    PUHCD_TD pDebugTD = (PUHCD_TD) m_pCPhysMem->PaToVa( m_vaFrameList[ dwFrameIndex ] & FRAME_LIST_POINTER_MASK );
    BOOL fFoundTD = FALSE;
    while ( pDebugTD != NULL ) {
        DEBUGCHK( pDebugTD->vaPrevIsochTD == NULL ||
                  pDebugTD->vaPrevIsochTD->vaNextTD == pDebugTD );
        DEBUGCHK( pDebugTD->vaNextTD == NULL ||
                  pDebugTD->vaNextTD->vaPrevIsochTD == pDebugTD );
        if ( pDebugTD == pTD ) {
            fFoundTD = TRUE;
        }
        pDebugTD = pDebugTD->vaNextTD;
    }
    DEBUGCHK( fFoundTD );
}
#endif // DEBUG

    if ( pTD->vaNextTD != NULL ) {
        DEBUGCHK( pTD->vaNextTD->Isochronous == 1 );
        DEBUGCHK( pTD->vaNextTD->vaPrevIsochTD == pTD );
        DEBUGCHK( pTD->HW_paLink == ( GetTDPhysAddr( pTD->vaNextTD )
                                            | TD_LINK_POINTER_TD
                                            | TD_LINK_POINTER_VALID ) );
        pTD->vaNextTD->vaPrevIsochTD = pTD->vaPrevIsochTD;
    }
#ifdef DEBUG
    else {
        // this TD should point into the QH tree
        DEBUGCHK( pTD->HW_paLink == ( GetQHPhysAddr( m_interruptQHTree[ QHTreeEntry( dwFrameIndex ) ] )
                                            | TD_LINK_POINTER_QH
                                            | TD_LINK_POINTER_VALID ) );
    } 
#endif // DEBUG

    if ( pTD->vaPrevIsochTD != NULL ) {
        // this TD is after another Isoch TD
        DEBUGCHK( pTD->vaPrevIsochTD->Isochronous == 1 );
        DEBUGCHK( pTD->vaPrevIsochTD->vaNextTD == pTD );
        DEBUGCHK( pTD->vaPrevIsochTD->HW_paLink == ( GetTDPhysAddr( pTD )
                                                        | TD_LINK_POINTER_VALID
                                                        | TD_LINK_POINTER_TD ) );
        pTD->vaPrevIsochTD->vaNextTD = pTD->vaNextTD;
        pTD->vaPrevIsochTD->HW_paLink = pTD->HW_paLink;
    } else {
        // this TD is right after the frame list
        DEBUGCHK( m_vaFrameList[ dwFrameIndex ] == ( GetTDPhysAddr( pTD )
                                                         | FRAME_LIST_POINTER_TD
                                                         | FRAME_LIST_POINTER_VALID ) );
        m_vaFrameList[ dwFrameIndex ] = pTD->HW_paLink;
    }

    pTD->vaPrevIsochTD = NULL;
    pTD->vaNextTD = NULL;
    // don't change pTD->HW_paLink because the host controller
    // may still be accessing this TD. Caller will take care of
    // this issue.
}
// ******************************************************************               
// Scope: protected static
void CUHCIFrame::HandleReclamationLoadChange( IN const BOOL fAddingTransfer  )
//
// Purpose: This function is called whenever transfers which use bandwidth
//          reclamation (high speed Bulk/Control) are added/removed.
//          If there are transfers for which reclamation is needed, this
//          function will clear the termination bit of m_pFinalQH. Otherwise,
//          it will set the termination bit to prevent infinite QH processing.
//
// Parameters: fAddingTransfer - if TRUE, a reclamation transfer is being added
//                               to the schedule. If FALSE, a reclamation transfer
//                               has just finished/aborted
//
// Returns: Nothing
//
// Notes: 
// ******************************************************************
{
    // important that this be static - this variable tracks the total
    // number of reclamation transfers in progress on the USB

    DEBUGMSG( ZONE_TRANSFER, (TEXT("+CUHCIFrame::HandleReclamationLoadChange - %s\n"), ((fAddingTransfer) ? TEXT("Add Reclamation Transfer") : TEXT("Remove Reclamation Transfer")) ) );

    EnterCriticalSection( &m_csQHScheduleLock );
#ifdef DEBUG
{
    UINT numReclamationQHs = 0;
    PUHCD_QH pQH = m_interruptQHTree[ 0 ]->vaNextQH;
    while ( pQH != m_pFinalQH ) {
        DEBUGCHK( pQH->vaPrevQH->vaNextQH == pQH &&
                  pQH->vaNextQH->vaPrevQH == pQH );
        pQH = pQH->vaNextQH;
        numReclamationQHs++;
    }
    DEBUGCHK( numReclamationTransfers <= numReclamationQHs );
}
#endif // DEBUG
    if ( fAddingTransfer ) {
        if ( numReclamationTransfers++ == 0 ) {
            // terminate bit should be set already, and we just need to turn it off.
            DEBUGCHK( m_pFinalQH->HW_paHLink == (GetQHPhysAddr( m_interruptQHTree[ 0 ] )
                                                     | QUEUE_HEAD_LINK_POINTER_QH
                                                     | QUEUE_HEAD_LINK_POINTER_TERMINATE) );
            m_pFinalQH->HW_paHLink ^= QUEUE_HEAD_LINK_POINTER_TERMINATE;
        }
        DEBUGCHK( m_pFinalQH->HW_paHLink == (GetQHPhysAddr( m_interruptQHTree[ 0 ] )
                                                     | QUEUE_HEAD_LINK_POINTER_QH
                                                     | QUEUE_HEAD_LINK_POINTER_VALID) );
    } else {
        DEBUGCHK( numReclamationTransfers > 0 );
        // terminate bit should not be set yet
        DEBUGCHK( m_pFinalQH->HW_paHLink == (GetQHPhysAddr( m_interruptQHTree[ 0 ] )
                                                     | QUEUE_HEAD_LINK_POINTER_QH
                                                     | QUEUE_HEAD_LINK_POINTER_VALID) );
        if ( --numReclamationTransfers == 0 ) {
            // there are no more reclamation transfers, so set terminate bit
            m_pFinalQH->HW_paHLink |= QUEUE_HEAD_LINK_POINTER_TERMINATE;
            DEBUGCHK( m_pFinalQH->HW_paHLink == (GetQHPhysAddr( m_interruptQHTree[ 0 ] )
                                                     | QUEUE_HEAD_LINK_POINTER_QH
                                                     | QUEUE_HEAD_LINK_POINTER_TERMINATE) );
        }
    }
    LeaveCriticalSection( &m_csQHScheduleLock );

    DEBUGMSG( ZONE_TRANSFER, (TEXT("-CUHCIFrame::HandleReclamationLoadChange - %s\n"), ((fAddingTransfer) ? TEXT("Add Reclamation Transfer") : TEXT("Remove Reclamation Transfer")) ) );
}



// ******************************************************************
// Scope: public 
CPipe::CPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
              IN const BOOL fIsLowSpeed ,IN CUHCIFrame *const pUHCIFrame)
//
// Purpose: constructor for CPipe
//
// Parameters: lpEndpointDescriptor - pointer to endpoint descriptor for
//                                    this pipe (assumed non-NULL)
//
//             fIsLowSpeed - indicates if this pipe is low speed
//
// Returns: Nothing.
//
// Notes: Most of the work associated with setting up the pipe
//        should be done via OpenPipe. The constructor actually
//        does very minimal work.
//
//        Do not modify static variables here!!!!!!!!!!!
// ******************************************************************
: CPipeAbs(lpEndpointDescriptor->bEndpointAddress )
, m_usbEndpointDescriptor( *lpEndpointDescriptor )
, m_pUHCIFrame(pUHCIFrame)
, m_fIsLowSpeed( !!fIsLowSpeed ) // want to ensure m_fIsLowSpeed is 0 or 1
, m_fIsHalted( FALSE )
, m_fTransferInProgress( FALSE )
, m_pLastTransfer ( NULL )
, m_bEndpointAddress( lpEndpointDescriptor->bEndpointAddress )
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("+CPipe::CPipe\n")) );
    // CPipe::Initialize should already have been called by now
    // to set up the schedule and init static variables
    DEBUGCHK( pUHCIFrame->m_debug_fInitializeAlreadyCalled );

    InitializeCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("-CPipe::CPipe\n")) );
}

// ******************************************************************
// Scope: public virtual 
CPipe::~CPipe( )
//
// Purpose: Destructor for CPipe
//
// Parameters: None
//
// Returns: Nothing.
//
// Notes:   Most of the work associated with destroying the Pipe
//          should be done via ClosePipe
//
//          Do not delete static variables here!!!!!!!!!!!
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("+CPipe::~CPipe\n")) );
    // transfers should be aborted or closed before deleting object
    DEBUGCHK( m_fTransferInProgress == FALSE );
    DeleteCriticalSection( &m_csPipeLock );
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("-CPipe::~CPipe\n")) );
}

// ******************************************************************               
// Scope: public 
HCD_REQUEST_STATUS CPipe::IssueTransfer( 
                                    IN const UCHAR address,
                                    IN LPTRANSFER_NOTIFY_ROUTINE const lpStartAddress,
                                    IN LPVOID const lpvNotifyParameter,
                                    IN const DWORD dwFlags,
                                    IN LPCVOID const lpvControlHeader,
                                    IN const DWORD dwStartingFrame,
                                    IN const DWORD dwFrames,
                                    IN LPCDWORD const aLengths,
                                    IN const DWORD dwBufferSize,     
                                    IN_OUT LPVOID const lpvClientBuffer,
                                    IN const ULONG paBuffer,
                                    IN LPCVOID const lpvCancelId,
                                    OUT LPDWORD const adwIsochErrors,
                                    OUT LPDWORD const adwIsochLengths,
                                    OUT LPBOOL const lpfComplete,
                                    OUT LPDWORD const lpdwBytesTransferred,
                                    OUT LPDWORD const lpdwError )
//
// Purpose: Issue a Transfer on this pipe
//
// Parameters: address - USB address to send transfer to
//
//             OTHER PARAMS - see comment in CUhcd::IssueTransfer
//
// Returns: requestOK if transfer issued ok, else requestFailed
//
// Notes:   
// ******************************************************************
{
    DEBUGMSG( ZONE_TRANSFER, (TEXT("+CPipe(%s)::IssueTransfer, address = %d\n"), GetPipeType(), address) );

    STransfer *pTransfer;
    HCD_REQUEST_STATUS  status = requestFailed;

    EnterCriticalSection( &m_csPipeLock );
    if ( m_fTransferInProgress ) {
        pTransfer = new STransfer;
        if (pTransfer == NULL) {
            DEBUGMSG( ZONE_ERROR, (TEXT("-CPipe(%s)::IssueTransfer cannot allocate transfer params!")
                                   TEXT(" returning Failed\n"), GetPipeType() ) );
            LeaveCriticalSection( &m_csPipeLock );
            return requestFailed;
        }
    } else
        pTransfer = &m_transfer;

#ifdef DEBUG
    memset( pTransfer, GARBAGE, sizeof( *pTransfer ) );
#endif // DEBUG
    // IssueTransfer params
    pTransfer->address = address;
    pTransfer->lpfnCallback = lpStartAddress;
    pTransfer->lpvCallbackParameter = lpvNotifyParameter;
    pTransfer->dwFlags = dwFlags;
    pTransfer->lpvControlHeader = lpvControlHeader;
    pTransfer->dwStartingFrame = dwStartingFrame;
    pTransfer->dwFrames = dwFrames;
    pTransfer->aLengths = aLengths;
    pTransfer->dwBufferSize = dwBufferSize;
    pTransfer->lpvClientBuffer = lpvClientBuffer;
    pTransfer->paClientBuffer = paBuffer;
    pTransfer->lpvCancelId = lpvCancelId;
    pTransfer->adwIsochErrors = adwIsochErrors;
    pTransfer->adwIsochLengths = adwIsochLengths;
    pTransfer->lpfComplete = lpfComplete;
    pTransfer->lpdwBytesTransferred = lpdwBytesTransferred;
    pTransfer->lpdwError = lpdwError;
    // non IssueTransfer params
    pTransfer->vaTDList = NULL;
    pTransfer->numTDsInList = 0;
    pTransfer->vaActualBuffer = PUCHAR( pTransfer->lpvClientBuffer );
    pTransfer->paActualBuffer = pTransfer->paClientBuffer;
    pTransfer->dwCurrentPermissions = GetCurrentPermissions();
    pTransfer->lpNextTransfer = NULL;

    // We must allocate the control header memory here so that cleanup works later.
    if (pTransfer->lpvControlHeader != NULL) {
        PUCHAR pSetup;
        // This must be a control transfer. It is asserted elsewhere,
        // but the worst case is we needlessly allocate some physmem.
        if ( !m_pUHCIFrame->m_pCPhysMem->AllocateMemory(
                                   DEBUG_PARAM( TEXT("IssueTransfer SETUP Buffer") )
                                   sizeof(USB_DEVICE_REQUEST),
                                   &pSetup,
                                   GetMemoryAllocationFlags() ) ) {
            DEBUGMSG( ZONE_WARNING, (TEXT("CPipe(%s)::IssueTransfer - no memory for SETUP buffer\n"), GetPipeType() ) );
            pTransfer->lpvControlHeader = NULL;
            pTransfer->paControlHeader = 0;
            goto doneIssueTransfer;
        }
        pTransfer->paControlHeader = m_pUHCIFrame->m_pCPhysMem->VaToPa( pSetup );
        PREFAST_DEBUGCHK( pSetup != NULL) ;  
        PREFAST_DEBUGCHK( pTransfer->paControlHeader != 0 );
    #ifdef DEBUG
        m_pUHCIFrame->m_debug_ControlExtraMemoryAllocated += sizeof( USB_DEVICE_REQUEST );
        DEBUGMSG( ZONE_TRANSFER, (TEXT("CPipe(Control)::IssueTransfer - alloc 1 control header, total bytes = %d\n"), m_pUHCIFrame->m_debug_ControlExtraMemoryAllocated ));
    #endif // DEBUG

        __try {
            memcpy( pSetup, PUCHAR( pTransfer->lpvControlHeader ), sizeof(USB_DEVICE_REQUEST) );
            pTransfer->lpvControlHeader = pSetup;
        } __except( EXCEPTION_EXECUTE_HANDLER ) {
            // bad lpvControlHeader
            pTransfer->lpvControlHeader = pSetup;  // so we can free what we allocated
            goto doneIssueTransfer;
        }
    }

    if ( AreTransferParametersValid(pTransfer) ) {
        __try { // initializing transfer status parameters
            *pTransfer->lpfComplete = FALSE;
            *pTransfer->lpdwBytesTransferred = 0;
            *pTransfer->lpdwError = USB_NOT_COMPLETE_ERROR;
        } __except( EXCEPTION_EXECUTE_HANDLER ) {
              goto doneIssueTransfer;
        }
    } else {
        DEBUGCHK( 0 ); // either we're checking params wrong,
                       // or the caller is passing bad arguments
        goto doneIssueTransfer;
    }

    // allocate Transfer Descriptors 
    DEBUGCHK( pTransfer->numTDsInList == 0 &&
              pTransfer->vaTDList == NULL );
    pTransfer->numTDsInList = GetNumTDsNeeded(pTransfer);
    DEBUGCHK( pTransfer->numTDsInList > 0 );
    if ( !m_pUHCIFrame->m_pCPhysMem->AllocateMemory(
                            DEBUG_PARAM( TEXT("IssueTransfer TDs") )
                            pTransfer->numTDsInList * sizeof( UHCD_TD ),
                            &pTransfer->vaTDList,
                            GetMemoryAllocationFlags() ) ) {
        DEBUGMSG( ZONE_WARNING, (TEXT("CPipe(%s)::IssueTransfer - no memory for TD list\n"), GetPipeType() ) );
        pTransfer->numTDsInList = 0;
        pTransfer->vaTDList = NULL;
        goto doneIssueTransfer;
    }
#ifdef DEBUG
    m_pUHCIFrame->m_debug_TDMemoryAllocated += pTransfer->numTDsInList * sizeof( UHCD_TD );
    DEBUGMSG( ZONE_TD, (TEXT("CPipe(%s)::IssueTransfer - alloc %d TDs, total bytes = %d\n"), GetPipeType(), pTransfer->numTDsInList, m_pUHCIFrame->m_debug_TDMemoryAllocated ) );
#endif // DEBUG

#ifdef DEBUG
    if ( pTransfer->dwFlags & USB_IN_TRANSFER ) {
        // I am leaving this in for two reasons:
        //  1. The memset ought to work even on zero bytes to NULL.
        //  2. Why would anyone really want to do a zero length IN?
        DEBUGCHK( pTransfer->dwBufferSize > 0 &&
                  pTransfer->lpvClientBuffer != NULL );
        __try { // IN buffer, trash it
            memset( PUCHAR( pTransfer->lpvClientBuffer ), GARBAGE, pTransfer->dwBufferSize );
        } __except( EXCEPTION_EXECUTE_HANDLER ) {
        }
    }
#endif // DEBUG

    // allocate data buffer if needed
    DEBUGCHK( pTransfer->vaActualBuffer == PUCHAR( pTransfer->lpvClientBuffer ) &&
              pTransfer->paActualBuffer == pTransfer->paClientBuffer );

    if ( pTransfer->dwBufferSize > 0 &&
         pTransfer->paClientBuffer == 0 ) { 

        PREFAST_DEBUGCHK( pTransfer->lpvClientBuffer != NULL );

        // ok, there's data on this transfer and the client
        // did not specify a physical address for the
        // buffer. So, we need to allocate our own.

        if ( !m_pUHCIFrame->m_pCPhysMem->AllocateMemory(
                                   DEBUG_PARAM( TEXT("IssueTransfer Buffer") )
                                   pTransfer->dwBufferSize,
                                   &pTransfer->vaActualBuffer, 
                                   GetMemoryAllocationFlags() ) ) {
            DEBUGMSG( ZONE_WARNING, (TEXT("CPipe(%s)::IssueTransfer - no memory for TD buffer\n"), GetPipeType() ) );
            pTransfer->vaActualBuffer = NULL;
            pTransfer->paActualBuffer = 0;
            goto doneIssueTransfer;
        }
        pTransfer->paActualBuffer = m_pUHCIFrame->m_pCPhysMem->VaToPa( pTransfer->vaActualBuffer );
        DEBUGCHK( pTransfer->vaActualBuffer != NULL &&
                  pTransfer->paActualBuffer != 0 );
    #ifdef DEBUG
        m_pUHCIFrame->m_debug_BufferMemoryAllocated += pTransfer->dwBufferSize;
        DEBUGMSG( ZONE_TD, (TEXT("CPipe(%s)::IssueTransfer - alloc buffer of %d, total bytes = %d\n"), GetPipeType(), pTransfer->dwBufferSize, m_pUHCIFrame->m_debug_BufferMemoryAllocated ) );
    #endif // DEBUG

        if ( !(pTransfer->dwFlags & USB_IN_TRANSFER) ) {
            __try { // copying client buffer for OUT transfer
                memcpy( pTransfer->vaActualBuffer, PUCHAR( pTransfer->lpvClientBuffer ), pTransfer->dwBufferSize );
            } __except( EXCEPTION_EXECUTE_HANDLER ) {
                  // bad lpvClientBuffer
                  goto doneIssueTransfer;
            }
        }
    }

#ifdef DEBUG
    if (m_fTransferInProgress) {
        DEBUGCHK( pTransfer != &m_transfer );
        DEBUGCHK( m_pLastTransfer != NULL && m_pLastTransfer->lpNextTransfer == NULL );
    } else {
        DEBUGCHK( pTransfer == &m_transfer );
        DEBUGCHK( m_pLastTransfer == NULL );
    }
#endif //DEBUG

    // fill in TDs but don't actually make it happen yet
    status = AddTransfer(pTransfer);
    
    if (status == requestOK) {
        if (m_fTransferInProgress) {
            PREFAST_DEBUGCHK( m_pLastTransfer != NULL);
            m_pLastTransfer->lpNextTransfer = pTransfer;
        }
        else
            // tell the HC about it
            status = ScheduleTransfer();
    }

doneIssueTransfer:
    DEBUGCHK( (status != requestOK) ||
              (status == requestOK && m_fTransferInProgress) );
    if ( status != requestOK ) {
        FreeTransferMemory(pTransfer);
        if (pTransfer != &m_transfer)
            delete pTransfer;
    } else
        m_pLastTransfer = pTransfer;
    
    LeaveCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_TRANSFER, (TEXT("-CPipe(%s)::IssueTransfer - address = %d, returing HCD_REQUEST_STATUS %d\n"), GetPipeType(), address, status) );
    return status;
}

// ******************************************************************
// Scope: public
HCD_REQUEST_STATUS CPipe::IsPipeHalted( OUT LPBOOL const lpbHalted )
//
// Purpose: Return whether or not this pipe is halted (stalled)
//
// Parameters: lpbHalted - pointer to BOOL which receives
//                         TRUE if pipe halted, else FALSE
//
// Returns: requestOK 
//
// Notes:  Caller should check for lpbHalted to be non-NULL
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("+CPipe(%s)::IsPipeHalted\n"), GetPipeType()) );

    DEBUGCHK( lpbHalted ); // should be checked by CUhcd

    EnterCriticalSection( &m_csPipeLock );
    if (lpbHalted)
        *lpbHalted = m_fIsHalted;
    LeaveCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("-CPipe(%s)::IsPipeHalted, *lpbHalted = %d, returning HCD_REQUEST_STATUS %d\n"), GetPipeType(), *lpbHalted, requestOK) );
    return requestOK;
}

// ******************************************************************
// Scope: public
void CPipe::ClearHaltedFlag( void )
//
// Purpose: Clears the pipe is halted flag
//
// Parameters: None
//
// Returns: Nothing 
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("+CPipe(%s)::ClearHaltedFlag\n"), GetPipeType() ) );

    EnterCriticalSection( &m_csPipeLock );
    DEBUGMSG( ZONE_WARNING && !m_fIsHalted, (TEXT("CPipe(%s)::ClearHaltedFlag - warning! Called on non-stalled pipe\n"), GetPipeType()) );
    m_fIsHalted = FALSE;
    LeaveCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("-CPipe(%s)::ClearHaltedFlag\n"), GetPipeType()) );
}


// ******************************************************************               
// Scope: protected virtual
DWORD CPipe::GetMemoryAllocationFlags( void ) const
//
// Purpose: Get flags for allocating memory from the CPhysMem class.
//          Descended pipes can over-ride this if they want to
//          specify different memory alloc flags (i.e. block or not,
//          high priority or not, etc)
//
// Parameters: None
//
// Returns: DWORD representing memory allocation flags
//
// Notes: 
// ******************************************************************
{
    // default choice - not high priority, no blocking
    return CPHYSMEM_FLAG_NOBLOCK;
}

// ******************************************************************               
// Scope: protected 
void CPipe::FreeTransferMemory( STransfer *pTransfer )
//
// Purpose: Free all of the memory associated with the current transfer.
//          For this function, that would be:
//                  - Any TDs allocated to carry the transfer request
//                  - Any data buffer allocated if the client passed
//                    in paClientBuffer == 0
//                  - A SETUP buffer for control transfers
//
// Parameters: None - (all are in m_transfer)
//
// Returns: Nothing
//
// Notes: Should only be called when a transfer is NOT in progress!!
// ******************************************************************
{
    if (pTransfer == NULL)
        pTransfer = &m_transfer;
    
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("+CPipe(%s)::FreeTransferMemory\n"), GetPipeType() ) );
    EnterCriticalSection( &m_csPipeLock );

    //DEBUGCHK( !m_fTransferInProgress ); // not a valid assertion when we can queue pending transfers

    if (pTransfer->lpvControlHeader != NULL) {
        DEBUGCHK( pTransfer->paControlHeader != 0 );
        DEBUGCHK( pTransfer->paControlHeader == m_pUHCIFrame->m_pCPhysMem->VaToPa(PUCHAR(pTransfer->lpvControlHeader)) );
        m_pUHCIFrame->m_pCPhysMem->FreeMemory( PUCHAR(pTransfer->lpvControlHeader),
                                 pTransfer->paControlHeader,
                                 GetMemoryAllocationFlags() );
    #ifdef DEBUG
        m_pUHCIFrame->m_debug_ControlExtraMemoryAllocated -= sizeof( USB_DEVICE_REQUEST );
        DEBUGMSG( ZONE_TRANSFER, (TEXT("CPipe(Control)::FreeTransferMemory - free 1 control header, total bytes = %d\n"), m_pUHCIFrame->m_debug_ControlExtraMemoryAllocated ));
    #endif // DEBUG
    }

    // Free Transfer Descriptor (TD) list
    if ( pTransfer->vaTDList != NULL ) {
        DEBUGCHK( pTransfer->numTDsInList > 0 );
        m_pUHCIFrame->m_pCPhysMem->FreeMemory( pTransfer->vaTDList,
                                 m_pUHCIFrame->m_pCPhysMem->VaToPa( pTransfer->vaTDList ),
                                 GetMemoryAllocationFlags() );
    #ifdef DEBUG
        m_pUHCIFrame->m_debug_TDMemoryAllocated -= pTransfer->numTDsInList * sizeof( UHCD_TD );
        DEBUGMSG( ZONE_TD, (TEXT("CPipe(%s)::FreeTransferMemory - free %d TDs, total bytes = %d\n"), GetPipeType(), pTransfer->numTDsInList, m_pUHCIFrame->m_debug_TDMemoryAllocated ) );
        DEBUGCHK( m_pUHCIFrame->m_debug_TDMemoryAllocated >= CPIPE_INITIALIZE_TD_MEMORY_NEEDED );
    #endif // DEBUG
    }
#ifdef DEBUG
    else {
        DEBUGCHK( pTransfer->numTDsInList == 0 );
    }
    pTransfer->vaTDList = PUCHAR( 0xdeadbeef );
    pTransfer->numTDsInList = 0xbeef;
#endif // DEBUG

    // Free data buffer, if we allocated one
    if ( pTransfer->paActualBuffer != 0 &&
         pTransfer->paActualBuffer != pTransfer->paClientBuffer ) {

        DEBUGCHK( pTransfer->vaActualBuffer != NULL &&
                  pTransfer->lpvClientBuffer != NULL &&
                  pTransfer->dwBufferSize > 0 );  // we didn't allocate memory if the xfer was 0 bytes

        m_pUHCIFrame->m_pCPhysMem->FreeMemory( pTransfer->vaActualBuffer,
                                 pTransfer->paActualBuffer,
                                 GetMemoryAllocationFlags() );
    #ifdef DEBUG
        m_pUHCIFrame->m_debug_BufferMemoryAllocated -= pTransfer->dwBufferSize;
        DEBUGMSG( ZONE_TD, (TEXT("CPipe(%s)::FreeTransferMemory - free buffer of %d, total bytes = %d\n"), GetPipeType(), pTransfer->dwBufferSize, m_pUHCIFrame->m_debug_BufferMemoryAllocated ) );
        DEBUGCHK( m_pUHCIFrame->m_debug_BufferMemoryAllocated >= 0 );
    #endif // DEBUG
    }
#ifdef DEBUG
    pTransfer->vaActualBuffer = PUCHAR( 0xdeadbeef );
    pTransfer->paActualBuffer = 0xdeadbeef;
#endif // DEBUG

    LeaveCriticalSection( &m_csPipeLock );
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("-CPipe(%s)::FreeTransferMemory\n"), GetPipeType() ) );
}


// ******************************************************************               
// Scope: public
CQueuedPipe::CQueuedPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                          IN const BOOL fIsLowSpeed,
                          IN CUhcd *const pCUhcd )
//
// Purpose: Constructor for CQueuedPipe
//
// Parameters: See CPipe::CPipe
//
// Returns: Nothing
//
// Notes: Do not modify static variables here!!
// ******************************************************************
: CPipe( lpEndpointDescriptor, fIsLowSpeed, pCUhcd)   // constructor for base class
, m_pCUhcd(pCUhcd)
, m_fIsReclamationPipe( FALSE ) // this gets set to TRUE later as need arises
, m_pPipeQH( NULL ) // queue head for this pipe
, m_dataToggleC( 1 ) // data toggle for this pipe
, m_dataToggleQ( 1 ) // data toggle for this pipe
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("+CQueuedPipe::CQueuedPipe\n")) );
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("-CQueuedPipe::CQueuedPipe\n")) );
}

// ******************************************************************               
// Scope: public virtual
CQueuedPipe::~CQueuedPipe( )
//
// Purpose: Destructor for CQueuedPipe
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: Do not modify static variables here!!
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("+CQueuedPipe::~CQueuedPipe\n")) );
    // queue should be freed via ClosePipe before calling destructor
    DEBUGCHK( m_pPipeQH == NULL );
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("-CQueuedPipe::~CQueuedPipe\n")) );
}
// ******************************************************************
// Scope: public
void CQueuedPipe::ClearHaltedFlag( void )
//
// Purpose: Clears the pipe is halted flag
//
// Parameters: None
//
// Returns: Nothing 
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("+CQueuedPipe(%s)::ClearHaltedFlag\n"), GetPipeType() ) );

    EnterCriticalSection( &m_csPipeLock );
    DEBUGMSG( ZONE_WARNING && !m_fIsHalted, (TEXT("CQueuedPipe(%s)::ClearHaltedFlag - warning! Called on non-stalled pipe\n"), GetPipeType()) );
    m_fIsHalted = FALSE;
    m_dataToggleC = 1; // Reset Data Toggle Bit after it stall.
    m_dataToggleQ = 1;
    LeaveCriticalSection( &m_csPipeLock );
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("-CQueuedPipe(%s)::ClearHaltedFlag\n"), GetPipeType()) );
}

// ******************************************************************               
// Scope: public (Implements CPipe::ClosePipe = 0)
HCD_REQUEST_STATUS CQueuedPipe::ClosePipe( void )
//
// Purpose: Abort any transfers associated with this pipe, and
//          remove its data structures from the schedule
//
// Parameters: None
//
// Returns: requestOK
//
// Notes: 
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE, (TEXT("+CQueuedPipe(%s)::ClosePipe\n"), GetPipeType() ) );

    // Don't enter CS before calling AbortTransfer, since
    // that function will need to leave the pipe CS
    // at a certain point to avoid deadlock, and
    // won't be able to do so if the CS is nested.

    while (m_fTransferInProgress)
        AbortTransfer( NULL, // callback function
                       NULL, // callback param
                       m_transfer.lpvCancelId );

    EnterCriticalSection( &m_csPipeLock );

    // transfer was either never issued, finished before we got to
    // this function, or aborted above.
    DEBUGCHK( !m_fTransferInProgress );

    // if this check fails, it's not fatal, but code shouldn't
    // call ClosePipe unless OpenPipe succeeded.
    DEBUGCHK( m_pPipeQH != NULL );
    if ( m_pPipeQH != NULL ) {
        DEBUGCHK( m_pPipeQH->vaVertTD == NULL &&
                  m_pPipeQH->HW_paVLink == QUEUE_ELEMENT_LINK_POINTER_TERMINATE );

        EnterCriticalSection( &(m_pUHCIFrame->m_csQHScheduleLock) );
    
        // next QH's previous is now m_pPipeQH->vaPrevQH
        PREFAST_DEBUGCHK( m_pPipeQH->vaNextQH != NULL );
        // recall interrupt m_pPipeQH goes into a binary tree, so it
        // is possible for next->prev not to refer to m_pPipeQH
        DEBUGCHK( m_pPipeQH->vaNextQH->vaPrevQH == m_pPipeQH ||
                  USB_ENDPOINT_TYPE_INTERRUPT == (m_usbEndpointDescriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) );
        m_pPipeQH->vaNextQH->vaPrevQH = m_pPipeQH->vaPrevQH;
        // prev QH's next is now m_pPipeQH->vaNextQH
        DEBUGCHK( m_pPipeQH->vaPrevQH->vaNextQH == m_pPipeQH );
        m_pPipeQH->vaPrevQH->vaNextQH = m_pPipeQH->vaNextQH;
        m_pPipeQH->vaPrevQH->HW_paHLink = m_pPipeQH->HW_paHLink;
        // we need to ensure the hardware is no longer processing this QH
        // before removing the hardware links. In the future, perhaps
        // this can be done on another thread.
        m_pCUhcd->CHW::WaitOneFrame();
        m_pPipeQH->HW_paHLink = QUEUE_HEAD_LINK_POINTER_TERMINATE;
    
        // make any needed changes to the m_interruptQHTree
        UpdateInterruptQHTreeLoad( (UCHAR) m_pPipeQH->dwInterruptTree.BranchIndex, -1 );
    
        LeaveCriticalSection( &m_pUHCIFrame->m_csQHScheduleLock );
    
        // free QH memory
        m_pUHCIFrame->m_pCPhysMem->FreeMemory( PUCHAR( m_pPipeQH ),
                                 GetQHPhysAddr( m_pPipeQH ),
                                 GetMemoryAllocationFlags() );
        m_pPipeQH = NULL;
    #ifdef DEBUG
        m_pUHCIFrame->m_debug_QHMemoryAllocated -= sizeof( UHCD_QH );
        DEBUGMSG( ZONE_QH, (TEXT("CQueuedPipe(%s)::ClosePipe - free 1 QH, total bytes = %d\n"), GetPipeType(), m_pUHCIFrame->m_debug_QHMemoryAllocated) );
        DEBUGCHK( m_pUHCIFrame->m_debug_QHMemoryAllocated >= CPIPE_INITIALIZE_QH_MEMORY_NEEDED );
    #endif // DEBUG
    }
    LeaveCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_PIPE, (TEXT("-CQueuedPipe(%s)::ClosePipe\n"), GetPipeType() ) );
    return requestOK;
}

// ******************************************************************               
// Scope: public (implements CPipe::AbortTransfer = 0)
HCD_REQUEST_STATUS CQueuedPipe::AbortTransfer( 
                                IN const LPTRANSFER_NOTIFY_ROUTINE lpCancelAddress,
                                IN const LPVOID lpvNotifyParameter,
                                IN LPCVOID lpvCancelId )
//
// Purpose: Abort any transfer on this pipe if its cancel ID matches
//          that which is passed in.
//
// Parameters: lpCancelAddress - routine to callback after aborting transfer
//
//             lpvNotifyParameter - parameter for lpCancelAddress callback
//
//             lpvCancelId - identifier for transfer to abort
//
// Returns: requestOK if transfer aborted
//          requestFailed if lpvCancelId doesn't match currently executing
//                 transfer, or if there is no transfer in progress
//
// Notes:
// ******************************************************************
{
    DEBUGMSG( ZONE_TRANSFER, (TEXT("+CQueuedPipe(%s)::AbortTransfer - lpvCancelId = 0x%x\n"), GetPipeType(), lpvCancelId) );

    HCD_REQUEST_STATUS status = requestFailed;
    BOOL               fLeftCS = FALSE;
    STransfer         *pTransfer;

    EnterCriticalSection( &m_csPipeLock );

    if (m_fTransferInProgress) {
        for (pTransfer = &m_transfer;
             pTransfer != NULL && pTransfer->lpvCancelId != lpvCancelId;
             pTransfer = pTransfer->lpNextTransfer)
            if (pTransfer->lpNextTransfer == m_pLastTransfer)
                // if anything gets removed, it'll be the last one in the list;
                // otherwise, we'll need to reset this later.
                m_pLastTransfer = pTransfer;
        if (pTransfer == NULL && m_pLastTransfer->lpNextTransfer != NULL)
            // oops - didn't find one, so reset my state
            m_pLastTransfer = m_pLastTransfer->lpNextTransfer;
    }

    if ( m_fTransferInProgress && pTransfer != NULL ) {
        BOOL fAbortQueue = FALSE;
        DEBUGCHK( pTransfer->lpvCancelId == lpvCancelId );
        DEBUGCHK( AreTransferParametersValid(pTransfer) );

        if (pTransfer == &m_transfer) {
            // the transfer is not yet done. Abort it...
            AbortQueue( );  // this really only aborts the current transfer
            fAbortQueue = TRUE;
        }

        FreeTransferMemory(pTransfer);

        // cache the callback info because *pTransfer will be transmogrified
        LPTRANSFER_NOTIFY_ROUTINE lpfnCompletion = pTransfer->lpfnCallback;
        LPVOID                    lpvParam = pTransfer->lpvCallbackParameter;

        STransfer *pNext = pTransfer->lpNextTransfer;
        if (pNext == m_pLastTransfer)
            m_pLastTransfer = pTransfer;
        if (pNext) {
            *pTransfer = *pNext; // yes, yes, it's a copy.
            delete pNext;
        } else {
            if (m_pLastTransfer == pTransfer) {
                // this is the only transfer in the queue
                DEBUGCHK(pTransfer == &m_transfer);
                m_fTransferInProgress = FALSE;
                m_pLastTransfer = NULL;
            } else {
                DEBUGCHK( m_pLastTransfer->lpNextTransfer == pTransfer );
                m_pLastTransfer->lpNextTransfer = NULL;
            }
        }
        // from this point on, pTransfer is no longer what it was.

        // need to exit crit sec after checking the flag but before
        // calling Remove to avoid deadlock with CheckForDoneTransfersThread
        if ( m_fTransferInProgress )
        {
            if (fAbortQueue)
                ScheduleTransfer();
            LeaveCriticalSection( &m_csPipeLock );
            fLeftCS = TRUE;
        } else {
            // the queue is now empty so the pipe is no longer busy
            DEBUGCHK( m_pLastTransfer == NULL );
            LeaveCriticalSection( &m_csPipeLock );
            fLeftCS = TRUE;
            m_pUHCIFrame->RemoveFromBusyPipeList( this );
        }

        // ok, transfer has been aborted. Perform callback if needed.
        if ( lpfnCompletion ) {
            __try { // calling the transfer complete callback
                ( *lpfnCompletion )( lpvParam );
            } __except( EXCEPTION_EXECUTE_HANDLER ) {
                DEBUGMSG( ZONE_ERROR, (TEXT("CQueuedPipe(%s)::AbortTransfer - exception executing callback function\n"), GetPipeType() ) );
            }
        }
        if ( lpCancelAddress ) {
            __try { // calling the Cancel function
                ( *lpCancelAddress )( lpvNotifyParameter );
            } __except( EXCEPTION_EXECUTE_HANDLER ) {
                DEBUGMSG( ZONE_ERROR, (TEXT("CQueuedPipe(%s)::AbortTransfer - exception executing callback function\n"), GetPipeType() ) );
            }
        }
        status = requestOK;
    }
    if ( !fLeftCS ) {
        LeaveCriticalSection( &m_csPipeLock );
    }
    DEBUGMSG( ZONE_TRANSFER, (TEXT("-CQueuedPipe(%s)::AbortTransfer - lpvCancelId = 0x%x, returning HCD_REQUEST_STATUS %d\n"), GetPipeType(), lpvCancelId, status) );
    return status;
}

// ******************************************************************               
// Scope: private
void  CQueuedPipe::AbortQueue( void ) 
//
// Purpose: Abort the current transfer (i.e., queue of TDs).
//
// Parameters: pQH - pointer to Queue Head for transfer to abort
//
// Returns: Nothing
//
// Notes: Assumes caller is in a position where we can safely
//        abort m_pPipeQH
// ******************************************************************
{
    DEBUGMSG( ZONE_TRANSFER, (TEXT("+CQueuedPipe(%s)::AbortQueue\n"), GetPipeType()) );

    EnterCriticalSection( &m_csPipeLock );

    PREFAST_DEBUGCHK( m_pPipeQH != NULL );
    DEBUGCHK( m_fTransferInProgress &&
              m_pPipeQH->vaVertTD != NULL );

    if ( m_fIsReclamationPipe && m_transfer.lpNextTransfer == NULL) {
        DEBUGCHK( m_usbEndpointDescriptor.wMaxPacketSize <= UHCD_MAX_RECLAMATION_PACKET_SIZE );
        DEBUGCHK( !m_fIsLowSpeed );
        m_pUHCIFrame->HandleReclamationLoadChange( FALSE ); // FALSE = removing transfer
    }
    m_pPipeQH->HW_paVLink = QUEUE_ELEMENT_LINK_POINTER_TERMINATE;
    PUHCD_TD pTD = m_pPipeQH->vaVertTD;
#ifdef DEBUG
    USHORT tdCount = 0;
#endif // DEBUG
    while ( pTD != NULL ) {
        // Leave the Active bit so we can correctly adjust dataToggle later.
        pTD->HW_paLink |= TD_LINK_POINTER_TERMINATE;
        pTD->InterruptOnComplete = 0;
        pTD = pTD->vaNextTD;
    #ifdef DEBUG
        tdCount++;
    #endif // DEBUG
    }
    DEBUGCHK( tdCount == m_transfer.numTDsInList ||
              tdCount == m_transfer.numTDsInList + 2 ); // extra 2 for control trans
    // set this again, in case the field was updated by the
    // completion of a TD
    m_pPipeQH->HW_paVLink = QUEUE_ELEMENT_LINK_POINTER_TERMINATE;

    // this Sleep is to ensure the USB hardware is no longer
    // processing the TDs of the queue. This will block the pipe
    // for one ms - potentially, this can block the CheckForDoneTransfersThread
    // and delay processing of other done pipes, and it will block
    // the calling thread. In the future, perhaps this can be done
    // in a different thread.
    m_pCUhcd->CHW::WaitOneFrame();

    //Now go through Vertical TD list to adjust data toggle
    pTD = m_pPipeQH->vaVertTD;
    while ( pTD != NULL && pTD->Active == 0 )
        pTD = pTD->vaNextTD;

    if(pTD != NULL) 
        m_dataToggleC = (UCHAR) pTD->DataToggle;
    //else it is already set correctly.

    // just to be absolutely sure...
    m_pPipeQH->HW_paVLink = QUEUE_ELEMENT_LINK_POINTER_TERMINATE;
    m_pPipeQH->vaVertTD = NULL;

    DWORD dwOldPermissions =  SetProcPermissions(m_transfer.dwCurrentPermissions);
    __try { // accessing caller's memory  
        *m_transfer.lpdwError = USB_CANCELED_ERROR;
        *m_transfer.lpfComplete = TRUE;
        *m_transfer.lpdwBytesTransferred = 0;
    } __except( EXCEPTION_EXECUTE_HANDLER ) {
          DEBUGMSG( ZONE_ERROR, (TEXT("CQueuedPipe(%s)::AbortQueue - exception setting transfer done status.\n"), GetPipeType() ) );
    }
    SetProcPermissions(dwOldPermissions);

    LeaveCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_TRANSFER, (TEXT("-CQueuedPipe(%s)::AbortQueue\n"), GetPipeType()));
}

// clean up the td list after a transfer completes short
BOOL CQueuedPipe::ProcessShortTransfer( PUHCD_TD pTD )
{
    DEBUGMSG(ZONE_TRANSFER, (_T("CQueuedPipe::ProcessShortTransfer: cleaning up\n")));
    
    // turn off further interrupts on this transfer just in case
    pTD->InterruptOnComplete = 0;
    // terminate the transfer
    m_pPipeQH->HW_paVLink = QUEUE_ELEMENT_LINK_POINTER_TERMINATE;

    // nothing left to do for the current transfer
    return FALSE;
}

// ******************************************************************               
// Scope: protected (Implements CPipe::CheckForDoneTransfers = 0)
BOOL CQueuedPipe::CheckForDoneTransfers( void )
//
// Purpose: Check if the transfer on this pipe is finished, and 
//          take the appropriate actions - i.e. remove the transfer
//          data structures and call any notify routines
//
// Parameters: None
//
// Returns: TRUE if transfer is done, else FALSE
//
// Notes:
// ******************************************************************
{
// This wouldn't be necessary if the length fields were declared to be signed:
#define UNENC_LEN(bf) (((DWORD)(bf) + 1) & 0x7FF)  // maps 0-x7fe to 1-x7ff and x7ff to 0

    DEBUGMSG( ZONE_TRANSFER, (TEXT("+CQueuedPipe(%s)::CheckForDoneTransfers\n"), GetPipeType() ) );

    EnterCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_TRANSFER && !m_fTransferInProgress, (TEXT("CQueuedPipe(%s)::CheckForDoneTransfers - queue was already finished on entry\n"), GetPipeType() ) );

    if ( m_fTransferInProgress ) {
        // this is the normal case where we want to check if the queue is done
        // and take appropriate action
    PREFAST_DEBUGCHK(m_pPipeQH != NULL);
    #ifdef DEBUG
        DEBUGCHK( AreTransferParametersValid() );

        DWORD dwOldPermissions1 =  SetProcPermissions(m_transfer.dwCurrentPermissions);
        __try { // checking if all parameters are as we expect...
                // (need a __try because of dereference of m_transfer.xyz params)
            DEBUGCHK( m_pPipeQH->vaVertTD != NULL &&
                      *m_transfer.lpdwBytesTransferred == 0 &&
                      *m_transfer.lpfComplete == FALSE &&
                      *m_transfer.lpdwError == USB_NOT_COMPLETE_ERROR );
        } __except( EXCEPTION_EXECUTE_HANDLER ) {
              // this shouldn't happen
        }
        SetProcPermissions(dwOldPermissions1);
    #endif // DEBUG

        DWORD dwUsbError = USB_NO_ERROR;
        DWORD dwDataTransferred = m_transfer.dwBufferSize;

        if ( m_pPipeQH->HW_paVLink & QUEUE_ELEMENT_LINK_POINTER_TERMINATE ) {
            // the QUEUE is done. All TDs should have executed ok
            DEBUGMSG( ZONE_TRANSFER, (TEXT("CQueuedPipe(%s)::CheckForDoneTransfers - QH terminate bit set\n"), GetPipeType() ) );
#ifdef DEBUG
            {
                PUHCD_TD pTD = m_pPipeQH->vaVertTD;
                DEBUGCHK( (PUHCD_TD)m_transfer.vaTDList == pTD );
                USHORT tdCount = 0;
                while ( pTD != NULL ) {
                    DEBUGCHK( pTD->Active == 0 &&
                              pTD->StatusField == TD_STATUS_NO_ERROR &&
                              UNENC_LEN(pTD->ActualLength) <= UNENC_LEN(pTD->MaxLength) &&
                              pTD->Isochronous == 0 &&
                              pTD->LowSpeedControl == (DWORD) m_fIsLowSpeed &&
                              pTD->Endpoint == (DWORD) (m_usbEndpointDescriptor.bEndpointAddress & TD_ENDPOINT_MASK) &&
                              pTD->Address == (DWORD) m_transfer.address );
                    if (UNENC_LEN(pTD->ActualLength) < UNENC_LEN(pTD->MaxLength))
                        pTD = PUHCD_TD( m_transfer.vaTDList + ((m_transfer.numTDsInList - 1) * sizeof(UHCD_TD)) );
                    else
                        pTD = pTD->vaNextTD;
                    tdCount++;
                }
                DEBUGCHK( tdCount <= m_transfer.numTDsInList );
            }
#endif // DEBUG
            m_dataToggleC = ((PUHCD_TD)m_transfer.vaTDList)[m_transfer.numTDsInList - 1].DataToggle;

            // IN transfer where client did not specify a buffer
            // for us to DMA into. Thus, we created our own internal
            // buffer, and now need to copy the data.
            if ( (m_transfer.dwFlags & USB_IN_TRANSFER) &&
                 m_transfer.paClientBuffer == 0 ) {
                DEBUGCHK( m_transfer.lpvClientBuffer != NULL &&
                          m_transfer.paActualBuffer != 0 &&
                          m_transfer.vaActualBuffer != NULL );

                DWORD dwOldPermissions = SetProcPermissions(m_transfer.dwCurrentPermissions);
                __try {
                    memcpy( PUCHAR( m_transfer.lpvClientBuffer ), m_transfer.vaActualBuffer, m_transfer.dwBufferSize );
                } __except( EXCEPTION_EXECUTE_HANDLER ) {
                      dwDataTransferred = 0;
                      dwUsbError = USB_CLIENT_BUFFER_ERROR;
                }
                SetProcPermissions(dwOldPermissions);
            }
            m_fTransferInProgress = FALSE;
        } else {
            // The Queue may be done, but not have the terminate bit set
            // if there was an error on the transfer or if the TD is the
            // last in a short transfer set. Find the TD that
            // the Queue is currently pointing to, and check it for
            // errors. Note that the queue may just have finished, so
            // we could end up with paCurrentTD == 0 (in which case 
            // we will check the Queue again after the next interrupt)
            ULONG paCurrentTD = m_pPipeQH->HW_paVLink & QUEUE_ELEMENT_LINK_POINTER_MASK;
            if ( paCurrentTD != 0 ) {
                PUHCD_TD pTD = (PUHCD_TD) m_pUHCIFrame->m_pCPhysMem->PaToVa( paCurrentTD );
                DEBUGCHK( (pTD->Address == m_transfer.address &&
                           (DWORD) pTD->Endpoint == (DWORD) (m_usbEndpointDescriptor.bEndpointAddress & TD_ENDPOINT_MASK)) );
                if ( pTD->Active == 0 ){
                    if( pTD->StatusField != TD_STATUS_NO_ERROR ) {
                        DEBUGMSG( ZONE_WARNING, (TEXT("CQueuedPipe(%s)::CheckForDoneTransfers - failure on TD 0x%x, address = %d, endpoint = %d, errorCounter = %d, status field = 0x%x\n"), GetPipeType(), pTD, pTD->Address, pTD->Endpoint, pTD->ErrorCounter, pTD->StatusField ) );
                        // turn off further interrupts on this transfer
                        pTD->InterruptOnComplete = 0;
                        // terminate the transfer
                        m_pPipeQH->HW_paVLink = QUEUE_ELEMENT_LINK_POINTER_TERMINATE;
                        // TODO - does this data count need to be accurate?
                        dwDataTransferred = 0;
                        // note the data toggle for the most recently completed TD (i.e., the one before this one)
                        m_dataToggleC = !pTD->DataToggle; // prev TD's toggle is complement of this one's
                        // this error code could be made more specific,
                        // depending on the exact contents of StatusField,
                        // but for now just return USB_NOT_COMPLETE_ERROR
                        // or USB_STALL_ERROR
                        if ( pTD->StatusField & TD_STATUS_STALLED ) {
                            // stalled is a special case, and the most serious error
                            if (GetType() ==TYPE_CONTROL)
                                m_fIsHalted=FALSE;
                            else
                                m_fIsHalted = TRUE;
                            dwUsbError = USB_STALL_ERROR;
                            m_dataToggleC = 1; // Reset Data Toggle Bit after it stall.
                            m_dataToggleQ = 1;
                        } else {
                            dwUsbError = USB_NOT_COMPLETE_ERROR;
                        }
                        m_fTransferInProgress = FALSE;

                    } else if (UNENC_LEN(pTD->ActualLength) < UNENC_LEN(pTD->MaxLength)) {

                        // This transfer is short. Complete it regardless but return the
                        // error condition if it wasn't expected to be short.
                        
                        //  if short packet is okay, we need to go through the TD list
                        PUHCD_TD pT = (PUHCD_TD)m_transfer.vaTDList;
                        dwDataTransferred = 0;
                        while(pT){
                            ASSERT(pT->StatusField == TD_STATUS_NO_ERROR);

                            dwDataTransferred += UNENC_LEN(pT->ActualLength);

                            //  we only traverse up to the current TD
                            if(pT == pTD)   break;

                            pT = pT->vaNextTD;
                        }

                        // note the data toggle for the *current* TD since it did [partially] complete
                        m_dataToggleC = pTD->DataToggle;

                        if ( ! (m_transfer.dwFlags & USB_SHORT_TRANSFER_OK) )
                            dwUsbError = USB_DATA_UNDERRUN_ERROR;

                        // IN transfer where client did not specify a buffer
                        // for us to DMA into. Thus, we created our own internal
                        // buffer, and now need to copy the data.
                        if (dwDataTransferred > 0 &&  
                            (m_transfer.dwFlags & USB_IN_TRANSFER) &&
                            m_transfer.paClientBuffer == 0 ) {
                            DEBUGCHK( m_transfer.dwBufferSize > 0 &&
                                      m_transfer.lpvClientBuffer != NULL &&
                                      m_transfer.paActualBuffer != 0 &&
                                      m_transfer.vaActualBuffer != NULL );

                            DWORD dwOldPermissions = SetProcPermissions(m_transfer.dwCurrentPermissions);
                            __try {
                                memcpy( PUCHAR( m_transfer.lpvClientBuffer ), m_transfer.vaActualBuffer, dwDataTransferred );
                            } __except( EXCEPTION_EXECUTE_HANDLER ) {
                                dwDataTransferred = 0;
                                dwUsbError = USB_CLIENT_BUFFER_ERROR;
                            }
                            SetProcPermissions(dwOldPermissions);
                        }

                        // clean up the transfer as appropriate
                        m_fTransferInProgress = ProcessShortTransfer(pTD);
                        if (dwUsbError != USB_NO_ERROR)
                            m_fTransferInProgress = FALSE;
                    }
                }
            }
        }
        if ( !m_fTransferInProgress ) {
            DEBUGMSG( ZONE_TRANSFER, (TEXT("CQueuedPipe(%s)::CheckForDoneTransfers - a transfer is now done\n"), GetPipeType() ) );
            DEBUGCHK( m_pPipeQH->HW_paVLink & QUEUE_ELEMENT_LINK_POINTER_TERMINATE );

            // the TDs/Buffers associated with the transfer
            // can now be freed
            FreeTransferMemory();

            DWORD dwOldPermissions =  SetProcPermissions(m_transfer.dwCurrentPermissions);
            __try { // setting transfer status and executing callback function
                *m_transfer.lpfComplete = TRUE;
                *m_transfer.lpdwError = dwUsbError;
                *m_transfer.lpdwBytesTransferred = dwDataTransferred;
                if ( m_transfer.lpfnCallback ) {
                    ( *m_transfer.lpfnCallback )( m_transfer.lpvCallbackParameter );
                }
            } __except( EXCEPTION_EXECUTE_HANDLER ) {
                  DEBUGMSG( ZONE_ERROR, (TEXT("CQueuedPipe(%s)::CheckForDoneTransfers - exception setting transfer status to complete\n"), GetPipeType() ) );
            }
            SetProcPermissions(dwOldPermissions);

            // this transfer is now officially done; are more pending?
            STransfer *pNext = m_transfer.lpNextTransfer;
            if (pNext) {
                DEBUGCHK(m_pLastTransfer != NULL);
                if (pNext == m_pLastTransfer)
                    m_pLastTransfer = &m_transfer;
                m_transfer = *pNext;    // copy for historical reasons; will fix someday.
                delete pNext;

                // and note that a transfer is still in progress (albeit a new one)
                m_fTransferInProgress = TRUE;
                ScheduleTransfer();
                
            } else {
                
                // this queue is now done - clean up and remove the transfer
                DEBUGMSG( ZONE_TRANSFER, (TEXT("CQueuedPipe(%s)::CheckForDoneTransfers - queue was processed and is now finished\n"), GetPipeType() ) );
                if ( m_fIsReclamationPipe ) {
                    // Queued transfers don't count against the reclamation load because
                    // they're not actually in the TD list. So we change the load only
                    // when the entire queue empties.
                    DEBUGCHK( m_usbEndpointDescriptor.wMaxPacketSize <= UHCD_MAX_RECLAMATION_PACKET_SIZE );
                    DEBUGCHK( !m_fIsLowSpeed );
                    m_pUHCIFrame->HandleReclamationLoadChange( FALSE ); // FALSE = removing transfer
                }

                m_pPipeQH->vaVertTD = NULL;

                DEBUGCHK(m_pLastTransfer == &m_transfer);
                m_fTransferInProgress = FALSE;
                m_pLastTransfer = NULL;
                m_dataToggleQ = m_dataToggleC;
#ifdef DEBUG
                memset( &m_transfer, GARBAGE, sizeof( m_transfer ) );
#endif // DEBUG
            }
        }
    }

    BOOL fTransferDone = !m_fTransferInProgress;
    LeaveCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_TRANSFER, (TEXT("-CQueuedPipe(%s)::CheckForDoneTransfers, returning BOOL %d\n"), GetPipeType(), fTransferDone) );
    return fTransferDone;
}


// ******************************************************************               
// Scope: protected virtual
void CQueuedPipe::UpdateInterruptQHTreeLoad( IN const UCHAR, // branch - ignored
                                             IN const int ) // deltaLoad - ignored
//
// Purpose: Change the load counts of all members in m_interruptQHTree
//          which are on branch "branch" by deltaLoad. This needs
//          to be done when interrupt QHs are added/removed to be able
//          to find optimal places to insert new queues
//
// Parameters: branch - indicates index into m_interruptQHTree array
//                      after which the branch point occurs (i.e. if
//                      a new queue is inserted after m_interruptQHTree[ 8 ],
//                      branch will be 8)
//
//             deltaLoad - amount by which to alter the Load counts 
//                         of affected m_interruptQHTree members
//
// Returns: Nothing
//
// Notes: Currently, there is nothing to do for BULK/CONTROL queues.
//        INTERRUPT should override this function
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("+CQueuedPipe(%s)::UpdateInterruptQHTreeLoad\n"), GetPipeType() ) );
    DEBUGMSG( ZONE_PIPE && ZONE_WARNING, (TEXT("CQueuedPipe(%s)::UpdateInterruptQHTreeLoad - doing nothing\n"), GetPipeType()) );
    DEBUGCHK( (m_usbEndpointDescriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) != USB_ENDPOINT_TYPE_INTERRUPT );
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("-CQueuedPipe(%s)::UpdateInterruptQHTreeLoad\n"), GetPipeType() ) );
}

// ******************************************************************               
// Scope: public
CBulkPipe::CBulkPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                      IN const BOOL fIsLowSpeed,
                      IN CUhcd *const pCUhcd)
//
// Purpose: Constructor for CBulkPipe
//
// Parameters: See CQueuedPipe::CQueuedPipe
//
// Returns: Nothing
//
// Notes: Do not modify static variables here!!
// ******************************************************************
: CQueuedPipe( lpEndpointDescriptor, fIsLowSpeed, pCUhcd ) // constructor for base class
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("+CBulkPipe::CBulkPipe\n")) );
    DEBUGCHK( m_usbEndpointDescriptor.bDescriptorType == USB_ENDPOINT_DESCRIPTOR_TYPE &&
              m_usbEndpointDescriptor.bLength >= sizeof( USB_ENDPOINT_DESCRIPTOR ) &&
              (m_usbEndpointDescriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_BULK );

    DEBUGCHK( !fIsLowSpeed ); // bulk pipe must be high speed

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("-CBulkPipe::CBulkPipe\n")) );
}

// ******************************************************************               
// Scope: public
CBulkPipe::~CBulkPipe( )
//
// Purpose: Destructor for CBulkPipe
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: Do not modify static variables here!!
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("+CBulkPipe::~CBulkPipe\n")) );
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("-CBulkPipe::~CBulkPipe\n")) );
}


// ******************************************************************               
// Scope: public (Implements CPipe::OpenPipe = 0)
HCD_REQUEST_STATUS CBulkPipe::OpenPipe( void )
//
// Purpose: Create the data structures necessary to conduct
//          transfers on this pipe
//
// Parameters: None
//
// Returns: requestOK - if pipe opened
//
//          requestFailed - if pipe was not opened
//
// Notes: 
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE, (TEXT("+CBulkPipe::OpenPipe\n") ) );

    HCD_REQUEST_STATUS status = requestFailed;

    EnterCriticalSection( &m_csPipeLock );

    // if this fails, someone is trying to open
    // an already opened pipe
    DEBUGCHK( m_pPipeQH == NULL );
    // if this fails, we have a low speed Bulk device
    // which is not allowed by the UHCI spec (sec 1.3)
    DEBUGCHK( !m_fIsLowSpeed );

    if ( m_pPipeQH == NULL && 
         !m_fIsLowSpeed &&
         m_pUHCIFrame->m_pCPhysMem->AllocateMemory( 
                              DEBUG_PARAM( TEXT("Bulk QH") )
                              sizeof( UHCD_QH ),
                              (PUCHAR*)&m_pPipeQH,
                              GetMemoryAllocationFlags() ) ) {
    #ifdef DEBUG
        m_pUHCIFrame->m_debug_QHMemoryAllocated += sizeof( UHCD_QH );
        DEBUGMSG( ZONE_QH, (TEXT("CBulkPipe::OpenPipe - alloc 1 QH, total bytes = %d\n"), m_pUHCIFrame->m_debug_QHMemoryAllocated) );
    #endif // DEBUG

        EnterCriticalSection( &m_pUHCIFrame->m_csQHScheduleLock );
        PUHCD_QH pInsertAfterQH = NULL;
        // check for Reclamation eligibility
#ifdef UHCD_DONT_ALLOW_RECLAMATION_FOR_CONTROL_PIPES
        m_fIsReclamationPipe = FALSE;
        // bulk QH goes right before the final low speed QH, because
        // its max packet size is too large for guaranteed reclamation
        pInsertAfterQH = m_interruptQHTree[ 0 ]->vaPrevQH;
        DEBUGCHK( pInsertAfterQH->vaNextQH == m_interruptQHTree[ 0 ] );
#else
       if ( m_usbEndpointDescriptor.wMaxPacketSize <= UHCD_MAX_RECLAMATION_PACKET_SIZE ) {
            m_fIsReclamationPipe = TRUE;
            // bulk QH goes right before the final QH so that
            // it is eligible for bandwidth reclamation
            pInsertAfterQH = m_pUHCIFrame->m_pFinalQH->vaPrevQH;
            DEBUGCHK( pInsertAfterQH->vaNextQH == m_pUHCIFrame->m_pFinalQH );
        } else {
            m_fIsReclamationPipe = FALSE;
            // bulk QH goes right before the final low speed QH, because
            // its max packet size is too large for guaranteed reclamation
            pInsertAfterQH = m_pUHCIFrame->m_interruptQHTree[ 0 ]->vaPrevQH;
            DEBUGCHK( pInsertAfterQH->vaNextQH == m_pUHCIFrame->m_interruptQHTree[ 0 ] );
        }
#endif

        InitializeQH( m_pPipeQH, // QH to initialize
                      pInsertAfterQH, // previous QH
                      pInsertAfterQH->HW_paHLink, // HW link to next QH
                      pInsertAfterQH->vaNextQH ); // next QH

        // Next QH's prev is now our QH
        pInsertAfterQH->vaNextQH->vaPrevQH = m_pPipeQH;
        // Previous QH's next is now our QH
        pInsertAfterQH->vaNextQH = m_pPipeQH;
        // Insert ourselves into the schedule
        pInsertAfterQH->HW_paHLink = GetQHPhysAddr( m_pPipeQH )
                                         | QUEUE_HEAD_LINK_POINTER_QH
                                         | QUEUE_HEAD_LINK_POINTER_VALID;

        UpdateInterruptQHTreeLoad( (UCHAR) m_pPipeQH->dwInterruptTree.BranchIndex, 1 );
        LeaveCriticalSection( &m_pUHCIFrame->m_csQHScheduleLock );

        status = requestOK;
    }

    LeaveCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_PIPE, (TEXT("-CBulkPipe::OpenPipe, returning HCD_REQUEST_STATUS %d\n"), status) );
    return status;
}

// ******************************************************************               
// Scope: private (Implements CPipe::GetNumTDsNeeded = 0)
USHORT CBulkPipe::GetNumTDsNeeded( const STransfer *pTransfer ) const 
//
// Purpose: Determine how many transfer descriptors (TDs) are needed
//          for a Bulk transfer of the given size
//
// Parameters: None
//
// Returns: USHORT representing # of TDs needed
//
// Notes: Assumes m_csPipeLock held
// ******************************************************************
{
    if (pTransfer == NULL)
        pTransfer = &m_transfer;
    
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("+CBulkPipe::GetNumTDsNeeded\n")) );

    DEBUGCHK( AreTransferParametersValid() );

    // Bulk transfer will send wMaxPacketSize of data each TD, until
    // the last TD, where less data (but > 0) can be sent

    USHORT tdsNeeded = USHORT( pTransfer->dwBufferSize / m_usbEndpointDescriptor.wMaxPacketSize );
    if ( pTransfer->dwBufferSize % m_usbEndpointDescriptor.wMaxPacketSize != 0 ) {
        tdsNeeded++;
    }
    if (tdsNeeded == 0) {
        DEBUGCHK( pTransfer->dwBufferSize == 0 );
        tdsNeeded = 1;
    }

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("-CBulkPipe::GetNumTDsNeeded, returning %d\n"), tdsNeeded ) );
    return tdsNeeded;
}

// ******************************************************************               
// Scope: private (Implements CPipe::AreTransferParametersValid = 0)
BOOL CBulkPipe::AreTransferParametersValid( const STransfer *pTransfer ) const 
//
// Purpose: Check whether this class' transfer parameters are valid.
//          This includes checking m_transfer, m_pPipeQH, etc
//
// Parameters: None (all parameters are vars of class)
//
// Returns: TRUE if parameters valid, else FALSE
//
// Notes: Assumes m_csPipeLock already held
// ******************************************************************
{
    if (pTransfer == NULL)
        pTransfer = &m_transfer;
    
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("+CBulkPipe::AreTransferParametersValid\n")) );

    // these parameters aren't used by CBulkPipe, so if they are non NULL,
    // it doesn't present a serious problem. But, they shouldn't have been
    // passed in as non-NULL by the calling driver.
    DEBUGCHK( pTransfer->adwIsochErrors == NULL && // ISOCH
              pTransfer->adwIsochLengths == NULL && // ISOCH
              pTransfer->aLengths == NULL && // ISOCH
              pTransfer->lpvControlHeader == NULL ); // CONTROL
    // this is also not a serious problem, but shouldn't happen in normal
    // circumstances. It would indicate a logic error in the calling driver.
    DEBUGCHK( !(pTransfer->lpfnCallback == NULL && pTransfer->lpvCallbackParameter != NULL) );
    // DWORD                     pTransfer->dwStartingFrame (ignored - ISOCH)
    // DWORD                     pTransfer->dwFrames (ignored - ISOCH)

    BOOL fValid = ( m_pPipeQH != NULL &&
                    pTransfer->address > 0 &&
                    pTransfer->address <= USB_MAX_ADDRESS &&
                    (pTransfer->lpvClientBuffer != NULL || pTransfer->dwBufferSize == 0) &&
                    // paClientBuffer could be 0 or !0
                    pTransfer->lpfComplete != NULL &&
                    pTransfer->lpdwBytesTransferred != NULL &&
                    pTransfer->lpdwError != NULL );

    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("-CBulkPipe::AreTransferParametersValid, returning BOOL %d\n"), fValid) );
    return fValid;
}

// ******************************************************************               
// Scope: private (Implements CPipe::ScheduleTransfer = 0)
HCD_REQUEST_STATUS CBulkPipe::ScheduleTransfer( void ) 
//
// Purpose: Schedule a USB Transfer on this pipe
//
// Parameters: None (all parameters are in m_transfer)
//
// Returns: requestOK if transfer issued ok, else requestFailed
//
// Notes: 
// ******************************************************************
{
    HCD_REQUEST_STATUS status = requestFailed;
    
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("+CBulkPipe::ScheduleTransfer\n")) );

    EnterCriticalSection( &m_csPipeLock );

    // this should already have been checked by CPipe::IssueTransfer
    DEBUGCHK( AreTransferParametersValid() );

    // specific to CBulkPipe
    DEBUGCHK( (m_fTransferInProgress || m_pPipeQH->vaVertTD == NULL) &&
              (m_pPipeQH->HW_paVLink & QUEUE_ELEMENT_LINK_POINTER_TERMINATE) &&
              m_transfer.vaTDList != NULL &&
              m_transfer.vaActualBuffer != NULL &&
              (m_transfer.paActualBuffer != 0 || m_transfer.dwBufferSize == 0) &&
              m_transfer.numTDsInList > 0 );

    // make sure data toggles are still correct
    PUHCD_TD pTD = (PUHCD_TD) m_transfer.vaTDList;
    if (pTD->DataToggle == m_dataToggleC) {
        // we were unlucky; adjust the TD queue
        DEBUGMSG(ZONE_TRANSFER & ZONE_VERBOSE, (TEXT("CBulkPipe::ScheduleTransfer adjusting data toggles\n")) );
        for (int i=0; i<m_transfer.numTDsInList; ++i)
            (pTD++)->DataToggle ^= 1;
    }

    // insert into schedule (FALSE = add to end of list)
    if ( m_fTransferInProgress || m_pUHCIFrame->AddToBusyPipeList( this, FALSE ) ) {
        m_pPipeQH->vaVertTD = PUHCD_TD( m_transfer.vaTDList );
        m_pPipeQH->HW_paVLink =
            GetTDPhysAddr( m_pPipeQH->vaVertTD ) | QUEUE_ELEMENT_LINK_POINTER_TD | QUEUE_ELEMENT_LINK_POINTER_VALID;

        if ( !m_fTransferInProgress && m_fIsReclamationPipe ) {
            DEBUGCHK( m_usbEndpointDescriptor.wMaxPacketSize <= UHCD_MAX_RECLAMATION_PACKET_SIZE );
            DEBUGCHK( !m_fIsLowSpeed );
            m_pUHCIFrame->HandleReclamationLoadChange( TRUE ); // TRUE = adding transfer
        }
        m_fTransferInProgress = TRUE;
        status = requestOK;
    }

    LeaveCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_WARNING && status != requestOK, (TEXT("CBulkPipe::ScheduleTransfer - unable to issue transfer!!!\n")) );
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("-CBulkPipe::ScheduleTransfer, returning HCD_REQUEST_STATUS %d\n"), status) );
    return status;
}

// ******************************************************************               
// Scope: private
HCD_REQUEST_STATUS CBulkPipe::AddTransfer( STransfer *pTransfer ) 
//
// Purpose: Schedule a 2nd (or 3rd or ...) USB Transfer on this pipe
//
// Parameters: an STransfer structure (which contains the transfer parameters)
//
// Returns: requestOK if transfer issued ok, else requestFailed
//
// Notes: 
// ******************************************************************
{
    DEBUGMSG( ZONE_TRANSFER, (TEXT("+CBulkPipe::AddTransfer\n")) );

    EnterCriticalSection( &m_csPipeLock );

    // this should already have been checked by CPipe::IssueTransfer
    DEBUGCHK( AreTransferParametersValid() );

    // specific to CBulkPipe
    DEBUGCHK( pTransfer->vaTDList != NULL &&
              pTransfer->vaActualBuffer != NULL &&
              (pTransfer->paActualBuffer != 0 || pTransfer->dwBufferSize == 0) &&
              pTransfer->numTDsInList > 0 );

    const DWORD         dwDataPID = (pTransfer->dwFlags & USB_IN_TRANSFER) ?
                                        TD_IN_PID : TD_OUT_PID;
    const UCHAR         endpoint = UCHAR( m_usbEndpointDescriptor.bEndpointAddress & TD_ENDPOINT_MASK );
    DWORD               dwTDOffset = 0;
    DWORD               dwBufferOffset = 0;
    USHORT              tdNumber = 0;
    BOOL                bShortPacketDetect = !!(pTransfer->dwFlags & USB_IN_TRANSFER);

    DEBUGMSG(ZONE_TRANSFER, (L"CBulkPipe::AddTransfer dwFlags(0x%X), packetsize=0x%X\n", pTransfer->dwFlags, m_usbEndpointDescriptor.wMaxPacketSize));

    while ( ++tdNumber < pTransfer->numTDsInList ) {
        PUHCD_TD vaThisTD = PUHCD_TD( pTransfer->vaTDList + dwTDOffset );
        PUHCD_TD vaNextTD = PUHCD_TD( pTransfer->vaTDList + dwTDOffset + sizeof( UHCD_TD ) );

        DEBUGCHK( dwBufferOffset + m_usbEndpointDescriptor.wMaxPacketSize < pTransfer->dwBufferSize );

        InitializeTD( vaThisTD, // TD to initialize
                      GetTDPhysAddr( vaNextTD )
                             | TD_LINK_POINTER_VALID
                             | TD_LINK_POINTER_TD
                             | TD_LINK_POINTER_BREADTH_FIRST, // HW Link
                      vaNextTD, // virt addr of next TD
                      0, // InterruptOnComplete,
                      0, // Isochronous,
                      m_fIsLowSpeed, // LowSpeedControl,
                      dwDataPID, // PID (IN/OUT/SETUP)
                      pTransfer->address, // Device Address,
                      endpoint, // Device endpoint
                      (m_dataToggleQ ^= 1), // Data Toggle (USB spec 8.5.1)
                      m_usbEndpointDescriptor.wMaxPacketSize - 1, // MaxLength (n-1) form
                      pTransfer->paActualBuffer + dwBufferOffset,  // phys addr of buffer
                      bShortPacketDetect);

        dwTDOffset += sizeof( UHCD_TD );
        dwBufferOffset += m_usbEndpointDescriptor.wMaxPacketSize;
    }
    DEBUGCHK( tdNumber == pTransfer->numTDsInList );
    DEBUGCHK( pTransfer->dwBufferSize - dwBufferOffset >= 0 &&
              pTransfer->dwBufferSize - dwBufferOffset <= m_usbEndpointDescriptor.wMaxPacketSize );
    const PUHCD_TD vaLastTD = PUHCD_TD( pTransfer->vaTDList + dwTDOffset );
    InitializeTD( vaLastTD, // TD to initialize
                  TD_LINK_POINTER_TERMINATE, // HW link
                  NULL, // virt addr of next TD
                  1, // InterruptOnComplete,
                  0, // Isochronous,
                  m_fIsLowSpeed, // LowSpeedControl,
                  dwDataPID, // PID (IN/OUT/SETUP)
                  pTransfer->address, // Device Address,
                  endpoint, // Device endpoint
                  (m_dataToggleQ ^= 1), // Data Toggle (USB spec 8.5.1)
                  pTransfer->dwBufferSize - dwBufferOffset - 1, // MaxLength (n-1) form
                  pTransfer->paActualBuffer + dwBufferOffset,   // phys addr of buffer
                  bShortPacketDetect);

    LeaveCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("-CBulkPipe::AddTransfer, returning success\n")) );
    return requestOK;
}

// ******************************************************************               
// Scope: public
CControlPipe::CControlPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                            IN const BOOL fIsLowSpeed,
                            IN CUhcd *const pCUhcd)
//
// Purpose: Constructor for CControlPipe
//
// Parameters: See CQueuedPipe::CQueuedPipe
//
// Returns: Nothing
//
// Notes: Do not modify static variables here!!
// ******************************************************************
: CQueuedPipe( lpEndpointDescriptor, fIsLowSpeed, pCUhcd ) // constructor for base class
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("+CControlPipe::CControlPipe\n")) );
    DEBUGCHK( m_usbEndpointDescriptor.bDescriptorType == USB_ENDPOINT_DESCRIPTOR_TYPE &&
              m_usbEndpointDescriptor.bLength >= sizeof( USB_ENDPOINT_DESCRIPTOR ) &&
              (m_usbEndpointDescriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_CONTROL );

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("-CControlPipe::CControlPipe\n")) );
}

// ******************************************************************               
// Scope: public
CControlPipe::~CControlPipe( )
//
// Purpose: Destructor for CControlPipe
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: Do not modify static variables here!!
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("+CControlPipe::~CControlPipe\n")) );
    // m_vaActualControlHeader is allocated in the same block of
    // memory as m_pPipeQH. So, when m_pPipeQH is freed, m_vaActualControlHeader
    // will be freed as well.
    //
    // m_pPipeQH should never have allocated ok in the first place, or
    // should have been freed by the time ClosePipe completes
    DEBUGCHK( m_pPipeQH == NULL );
    //m_vaActualControlHeader <- don't need to touch this
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("-CControlPipe::~CControlPipe\n")) );
}

// ******************************************************************               
// Scope: public (Implements CPipe::OpenPipe = 0)
HCD_REQUEST_STATUS CControlPipe::OpenPipe( void )
//
// Purpose: Create the data structures necessary to conduct
//          transfers on this pipe
//
// Parameters: None
//
// Returns: requestOK - if pipe opened
//
//          requestFailed - if pipe was not opened
//
// Notes: 
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE, (TEXT("+CControlPipe::OpenPipe\n") ) );

    HCD_REQUEST_STATUS status = requestFailed;

    EnterCriticalSection( &m_csPipeLock );

    // if this fails, someone is trying to open
    // an already opened pipe
    DEBUGCHK( m_pPipeQH == NULL );

    // We allocate 1 QH plus an additional chunk of memory for
    // the control header. All of the memory will be freed when
    // m_pPipeQH is freed.
    if ( m_pPipeQH == NULL && 
         m_pUHCIFrame->m_pCPhysMem->AllocateMemory( 
                                DEBUG_PARAM( TEXT("Control QH + Header") )
                                sizeof( UHCD_QH ),
                                (PUCHAR*)&m_pPipeQH,
                                GetMemoryAllocationFlags() ) ) {

    #ifdef DEBUG
        m_pUHCIFrame->m_debug_QHMemoryAllocated += sizeof( UHCD_QH );
        DEBUGMSG( ZONE_QH, (TEXT("CControlPipe::OpenPipe - alloc 1 QH, total bytes = %d\n"), m_pUHCIFrame->m_debug_QHMemoryAllocated) );
    #endif // DEBUG

        EnterCriticalSection( &m_pUHCIFrame->m_csQHScheduleLock );

        // UHCI allows bandwidth reclamation on high speed
        // control transfers. The advantage of not defining the
        // UHCD_DONT_ALLOW_RECLAMATION_FOR_CONTROL_PIPES flag is
        // that control transfers may complete much faster,
        // because multiple TDs may complete per frame. The
        // disadvantage is that if there are any Bulk Transfers
        // whose max packet size is > UHCD_MAX_RECLAMATION_PACKET_SIZE,
        // then they will execute BEFORE the reclamation control
        // transfer. Hence, the control transfer *could* be delayed
        // for several frames *if* the bulk TD carries a significant
        // amount of data, and there isn't enough bandwidth left to
        // execute the control TD.
#ifdef UHCD_DONT_ALLOW_RECLAMATION_FOR_CONTROL_PIPES
        // This is set to FALSE in the constructor
        DEBUGCHK( !m_fIsReclamationPipe );
        PUHCD_QH pInsertAfterQH = m_pUHCIFrame->m_interruptQHTree[ 1 ];
#else // if !defined( UHCD_DONT_ALLOW_RECLAMATION_FOR_CONTROL_PIPES )
        PUHCD_QH pInsertAfterQH = NULL;
        if ( !m_fIsLowSpeed &&
             m_usbEndpointDescriptor.wMaxPacketSize <= UHCD_MAX_RECLAMATION_PACKET_SIZE ) {
            m_fIsReclamationPipe = TRUE;
            pInsertAfterQH = m_pUHCIFrame->m_interruptQHTree[ 0 ];
        } else {
            // This is set to FALSE in the constructor
            DEBUGCHK( !m_fIsReclamationPipe );
            pInsertAfterQH = m_pUHCIFrame->m_interruptQHTree[ 1 ];
        }
#endif // UHCD_DONT_ALLOW_RECLAMATION_FOR_CONTROL_PIPES

        PREFAST_DEBUGCHK( pInsertAfterQH != NULL);
        PREFAST_DEBUGCHK( pInsertAfterQH->vaNextQH != NULL );
        DEBUGCHK( pInsertAfterQH->vaNextQH->vaPrevQH == pInsertAfterQH );

        InitializeQH( m_pPipeQH, // QH to initialize
                      pInsertAfterQH, // previous QH
                      pInsertAfterQH->HW_paHLink, // HW link to next QH
                      pInsertAfterQH->vaNextQH ); // next QH

        // Next QH's prev is now our QH
        pInsertAfterQH->vaNextQH->vaPrevQH = m_pPipeQH;
        // Previous QH's next is now our QH
        pInsertAfterQH->vaNextQH = m_pPipeQH;
        // Insert ourselves into the schedule
        pInsertAfterQH->HW_paHLink = GetQHPhysAddr( m_pPipeQH )
                                         | QUEUE_HEAD_LINK_POINTER_QH
                                         | QUEUE_HEAD_LINK_POINTER_VALID;

        UpdateInterruptQHTreeLoad( (UCHAR) m_pPipeQH->dwInterruptTree.BranchIndex, 1 );
        LeaveCriticalSection( &m_pUHCIFrame->m_csQHScheduleLock );

        status = requestOK;
    }
    LeaveCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_PIPE, (TEXT("-CControlPipe::OpenPipe, returning HCD_REQUEST_STATUS %d\n"), status) );
    return status;
}

// ******************************************************************
// Scope: public
void CControlPipe::ChangeMaxPacketSize( IN const USHORT wMaxPacketSize )
//
// Purpose: Update the max packet size for this pipe. This should
//          ONLY be done for control endpoint 0 pipes. When the endpoint0
//          pipe is first opened, it has a max packet size of 
//          ENDPOINT_ZERO_MIN_MAXPACKET_SIZE. After reading the device's
//          descriptor, the device attach procedure can update the size.
//
// Parameters: wMaxPacketSize - new max packet size for this pipe
//
// Returns: Nothing
//
// Notes:   This function should only be called by the Hub AttachDevice
//          procedure
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("+CControlPipe::ChangeMaxPacketSize - new wMaxPacketSize = %d\n"), wMaxPacketSize) );

    EnterCriticalSection( &m_csPipeLock );

    // this pipe should be for endpoint 0, control pipe
    DEBUGCHK( (m_usbEndpointDescriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_CONTROL &&
              (m_usbEndpointDescriptor.bEndpointAddress & TD_ENDPOINT_MASK) == 0 );
    // update should only be called if the old address was ENDPOINT_ZERO_MIN_MAXPACKET_SIZE
    DEBUGCHK( m_usbEndpointDescriptor.wMaxPacketSize == ENDPOINT_ZERO_MIN_MAXPACKET_SIZE );
    // this function should only be called if we are increasing the max packet size.
    // in addition, the USB spec 1.0 section 9.6.1 states only the following
    // wMaxPacketSize are allowed for endpoint 0
    DEBUGCHK( wMaxPacketSize > ENDPOINT_ZERO_MIN_MAXPACKET_SIZE &&
              (wMaxPacketSize == 16 ||
               wMaxPacketSize == 32 ||
               wMaxPacketSize == 64) );
    DEBUGCHK( wMaxPacketSize <= UHCD_MAX_RECLAMATION_PACKET_SIZE );

    m_usbEndpointDescriptor.wMaxPacketSize = wMaxPacketSize;

    LeaveCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("-CControlPipe::ChangeMaxPacketSize - new wMaxPacketSize = %d\n"), wMaxPacketSize) );
}

// ******************************************************************               
// Scope: private (Implements CPipe::GetNumTDsNeeded = 0)
USHORT CControlPipe::GetNumTDsNeeded( const STransfer *pTransfer ) const 
//
// Purpose: Determine how many transfer descriptors (TDs) are needed
//          for a Control transfer of the given size
//
// Parameters: None
//
// Returns: USHORT representing # of TDs needed
//
// Notes: Assumes m_csPipeLock held
// ******************************************************************
{
    if (pTransfer == NULL)
        pTransfer = &m_transfer;
    
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("+CControlPipe::GetNumTDsNeeded\n")) );

    DEBUGCHK( AreTransferParametersValid() );

    // Control transfer will send wMaxPacketSize of data each TD, until
    // the last TD, where less data (but > 0) can be sent. Two additional
    // TDs are needed for the setup/status phases

    USHORT tdsNeeded = USHORT( pTransfer->dwBufferSize / m_usbEndpointDescriptor.wMaxPacketSize );
    if ( pTransfer->dwBufferSize % m_usbEndpointDescriptor.wMaxPacketSize != 0 ) {
        tdsNeeded++;
    }
    tdsNeeded += 2;

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("-CControlPipe::GetNumTDsNeeded, returning %d\n"), tdsNeeded ) );
    return tdsNeeded;
}

// ******************************************************************               
// Scope: private (Implements CPipe::AreTransferParametersValid = 0)
BOOL CControlPipe::AreTransferParametersValid( const STransfer *pTransfer ) const 
//
// Purpose: Check whether this class' transfer parameters are valid.
//          This includes checking m_transfer, m_pPipeQH, etc
//
// Parameters: None (all parameters are vars of class)
//
// Returns: TRUE if parameters valid, else FALSE
//
// Notes: Assumes m_csPipeLock already held
// ******************************************************************
{
    if (pTransfer == NULL)
        pTransfer = &m_transfer;
    
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("+CControlPipe::AreTransferParametersValid\n")) );

    // these parameters aren't used by CControlPipe, so if they are non NULL,
    // it doesn't present a serious problem. But, they shouldn't have been
    // passed in as non-NULL by the calling driver.
    DEBUGCHK( pTransfer->adwIsochErrors == NULL && // ISOCH
              pTransfer->adwIsochLengths == NULL && // ISOCH
              pTransfer->aLengths == NULL ); // ISOCH
    // this is also not a serious problem, but shouldn't happen in normal
    // circumstances. It would indicate a logic error in the calling driver.
    DEBUGCHK( !(pTransfer->lpfnCallback == NULL && pTransfer->lpvCallbackParameter != NULL) );
    // DWORD                     pTransfer->dwStartingFrame; (ignored - ISOCH)
    // DWORD                     pTransfer->dwFrames; (ignored - ISOCH)

    BOOL fValid = ( m_pPipeQH != NULL &&
                    pTransfer->address <= USB_MAX_ADDRESS &&
                    pTransfer->lpvControlHeader != NULL &&
                    pTransfer->lpfComplete != NULL &&
                    pTransfer->lpdwBytesTransferred != NULL &&
                    pTransfer->lpdwError != NULL );
    if ( fValid ) {
        if ( pTransfer->dwFlags & USB_IN_TRANSFER ) {
            fValid = (pTransfer->lpvClientBuffer != NULL &&
                      // paClientBuffer could be 0 or !0
                      pTransfer->dwBufferSize > 0);
        } else {
            fValid = ( (pTransfer->lpvClientBuffer == NULL && 
                        pTransfer->paClientBuffer == 0 &&
                        pTransfer->dwBufferSize == 0) ||
                       (pTransfer->lpvClientBuffer != NULL &&
                        // paClientBuffer could be 0 or !0
                        pTransfer->dwBufferSize > 0) );
        }
    }

    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("-CControlPipe::AreTransferParametersValid, returning BOOL %d\n"), fValid) );
    return fValid;
}

// ******************************************************************               
// Scope: private (Implements CPipe::ScheduleTransfer = 0)
HCD_REQUEST_STATUS CControlPipe::ScheduleTransfer( void ) 
//
// Purpose: Schedule a USB Transfer on this pipe
//
// Parameters: None (all parameters are in m_transfer)
//
// Returns: requestOK if transfer issued ok, else requestFailed
//
// Notes: 
// ******************************************************************
{
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("+CControlPipe::ScheduleTransfer\n")) );
    
    EnterCriticalSection( &m_csPipeLock );
    
    // this should already have been checked by CPipe::IssueTransfer
    DEBUGCHK( AreTransferParametersValid() );

    // specific to CControlPipe
    DEBUGCHK( (m_fTransferInProgress || m_pPipeQH->vaVertTD == NULL) &&
              (m_pPipeQH->HW_paVLink & QUEUE_ELEMENT_LINK_POINTER_TERMINATE) &&
              m_transfer.paControlHeader != 0 );
    
    HCD_REQUEST_STATUS status = requestFailed;

    // insert into schedule (FALSE = add to end of list)
    if ( m_fTransferInProgress || m_pUHCIFrame->AddToBusyPipeList( this, FALSE ) ) {
        m_pPipeQH->vaVertTD = PUHCD_TD( m_transfer.vaTDList );
        m_pPipeQH->HW_paVLink =
            GetTDPhysAddr( m_pPipeQH->vaVertTD ) | QUEUE_ELEMENT_LINK_POINTER_TD | QUEUE_ELEMENT_LINK_POINTER_VALID;
        if ( !m_fTransferInProgress && m_fIsReclamationPipe ) {
            DEBUGCHK( m_usbEndpointDescriptor.wMaxPacketSize <= UHCD_MAX_RECLAMATION_PACKET_SIZE );
            DEBUGCHK( !m_fIsLowSpeed );
            m_pUHCIFrame->HandleReclamationLoadChange( TRUE ); // TRUE = adding transfer
        }
        m_fTransferInProgress = TRUE;
        status = requestOK;
    }
    
    LeaveCriticalSection( &m_csPipeLock );
    
    DEBUGMSG( ZONE_WARNING && status != requestOK, (TEXT("CControlPipe::ScheduleTransfer - unable to issue transfer!!!\n")) );
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("-CControlPipe::ScheduleTransfer, returning HCD_REQUEST_STATUS %d\n"), status) );
    return status;
}

// ******************************************************************               
// Scope: private (Implements CPipe::AddTransfer = 0)
HCD_REQUEST_STATUS CControlPipe::AddTransfer( STransfer *pTransfer ) 
//
// Purpose: Schedule a USB Transfer on this pipe
//
// Parameters: an STransfer structure (which contains the transfer parameters)
//
// Returns: requestOK if transfer issued ok, else requestFailed
//
// Notes: 
// ******************************************************************
{
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("+CControlPipe::AddTransfer\n")) );
    
    EnterCriticalSection( &m_csPipeLock );
    
    // this should already have been checked by CPipe::IssueTransfer
    DEBUGCHK( AreTransferParametersValid() );

    // specific to CControlPipe
    DEBUGCHK( (m_fTransferInProgress || m_pPipeQH->vaVertTD == NULL) &&
              pTransfer->paControlHeader != 0 );
    
    HCD_REQUEST_STATUS  status = requestFailed;

    const UCHAR         endpoint = UCHAR( m_usbEndpointDescriptor.bEndpointAddress & TD_ENDPOINT_MASK );
    DWORD               dwDataPID; // set below
    DWORD               dwStatusPID; // set below

    DWORD               dwTDOffset = sizeof(UHCD_TD);
    DWORD               dwBufferOffset = 0;
    PUHCD_TD            pSetupTD = NULL; // set below
    PUHCD_TD            pStatusTD = NULL; // set below
    BOOL                bShortPacketDetect;

    if ( pTransfer->dwFlags & USB_IN_TRANSFER ) {
        dwDataPID = TD_IN_PID;
        bShortPacketDetect = 1;
        dwStatusPID = TD_OUT_PID; // opposite of data direction
    } else {
        dwDataPID = TD_OUT_PID;
        bShortPacketDetect = 0;  // should only set this on IN packets, per UHCI spec
        dwStatusPID = TD_IN_PID; // opposite of data direction
    }

    // start with the setup TD
    pSetupTD = PUHCD_TD( pTransfer->vaTDList );
    InitializeTD( pSetupTD, // TD to initialize
                  GetTDPhysAddr( pSetupTD+1 )
                             | TD_LINK_POINTER_VALID
                             | TD_LINK_POINTER_TD
                             | TD_LINK_POINTER_BREADTH_FIRST, // HW Link
                  pSetupTD+1, // virt addr of next TD
                  0, // InterruptOnComplete,
                  0, // Isochronous,
                  m_fIsLowSpeed, // LowSpeedControl,
                  TD_SETUP_PID, // PID 
                  pTransfer->address, // Device Address,
                  endpoint, // Device endpoint
                  0, // Data Toggle (USB spec 8.5.2)
                  sizeof( USB_DEVICE_REQUEST ) - 1, // MaxLength in (n-1) form
                  pTransfer->paControlHeader ); // phys addr of buffer

    if ( pTransfer->dwBufferSize > 0 ) {
        DEBUGCHK( pTransfer->numTDsInList > 2 &&
                  pTransfer->lpvClientBuffer != NULL &&
                  pTransfer->vaActualBuffer != NULL &&
                  pTransfer->paActualBuffer != 0 &&
                  pTransfer->vaTDList != NULL );

        USHORT tdNumber = 0;
        while ( ++tdNumber <= pTransfer->numTDsInList - 2 ) {
            PUHCD_TD vaThisTD = PUHCD_TD( pTransfer->vaTDList + dwTDOffset );
            PUHCD_TD vaNextTD = PUHCD_TD( pTransfer->vaTDList + dwTDOffset + sizeof( UHCD_TD ) );

            DWORD   dwThisTDBufferSize;
            if ( tdNumber < pTransfer->numTDsInList - 2 ) {
                dwThisTDBufferSize = m_usbEndpointDescriptor.wMaxPacketSize;
                DEBUGCHK( dwBufferOffset + dwThisTDBufferSize < pTransfer->dwBufferSize );
            } else {
                DEBUGCHK( tdNumber == pTransfer->numTDsInList - 2 );
                dwThisTDBufferSize = pTransfer->dwBufferSize - dwBufferOffset;
                DEBUGCHK( dwThisTDBufferSize > 0 &&
                          dwThisTDBufferSize <= m_usbEndpointDescriptor.wMaxPacketSize );
            }

            InitializeTD( vaThisTD, // TD to initialize
                          GetTDPhysAddr( vaNextTD )
                                     | TD_LINK_POINTER_VALID
                                     | TD_LINK_POINTER_TD
                                     | TD_LINK_POINTER_BREADTH_FIRST, // HW Link
                          vaNextTD, // virt addr of next TD
                          0, // InterruptOnComplete,
                          0, // Isochronous,
                          m_fIsLowSpeed, // LowSpeedControl,
                          dwDataPID, // PID (IN/OUT)
                          pTransfer->address, // Device Address,
                          endpoint, // Device endpoint
                          tdNumber & 1, // Data Toggle (USB spec 8.5.2)
                          dwThisTDBufferSize - 1, // MaxLength (n-1) form
                          pTransfer->paActualBuffer + dwBufferOffset, // phys addr of buffer
                          bShortPacketDetect);

            dwTDOffset += sizeof( UHCD_TD );
            dwBufferOffset += dwThisTDBufferSize;
        }
        DEBUGCHK( tdNumber == pTransfer->numTDsInList - 1 );
        DEBUGCHK( dwBufferOffset == pTransfer->dwBufferSize );
    }
    #ifdef DEBUG
    else {
        DEBUGCHK( pTransfer->numTDsInList == 2 &&
                  pTransfer->dwBufferSize == 0 &&
                  pTransfer->lpvClientBuffer == NULL &&
                  pTransfer->paClientBuffer == 0 &&
                  pTransfer->vaActualBuffer == NULL &&
                  pTransfer->paActualBuffer == 0 &&
                  !(pTransfer->dwFlags & USB_IN_TRANSFER) );
        DEBUGCHK( dwTDOffset == sizeof(UHCD_TD) &&
                  dwBufferOffset == 0 );
    }
    #endif // DEBUG

    // now for the status TD
    pStatusTD = PUHCD_TD( pTransfer->vaTDList + dwTDOffset );
    dwTDOffset += sizeof( UHCD_TD );
    InitializeTD( pStatusTD, // TD to initialize
                  TD_LINK_POINTER_TERMINATE, // HW link
                  NULL, // virt addr of Next TD
                  1, // InterruptOnComplete
                  0, // Isochronous
                  m_fIsLowSpeed, // LowSpeedControl
                  dwStatusPID, // PID (IN/OUT)
                  pTransfer->address, // Device Address
                  endpoint, // Device endpoint
                  1, // Data Toggle (USB spec 8.5.2)
                  TD_MAXLENGTH_NULL_BUFFER, // No data on status TD
                  0 ); // phys addr of data buffer

    status = requestOK;
    
    LeaveCriticalSection( &m_csPipeLock );
    
    DEBUGMSG( ZONE_WARNING && status != requestOK, (TEXT("CControlPipe::AddTransfer - unable to issue transfer!!!\n")) );
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("-CControlPipe::AddTransfer, returning HCD_REQUEST_STATUS %d\n"), status) );
    return status;
}


// handle completion of short transfers
BOOL CControlPipe::ProcessShortTransfer( PUHCD_TD pTD )
{
    BOOL fTransferInProgress;
    
    if ((m_transfer.dwFlags & USB_SHORT_TRANSFER_OK)) {
        PUHCD_TD vaLastTD = PUHCD_TD( m_transfer.vaTDList + ((m_transfer.numTDsInList - 1) * sizeof(UHCD_TD)) );
        DEBUGCHK(vaLastTD != pTD);
        DEBUGCHK(vaLastTD->InterruptOnComplete != 0);
        DEBUGMSG(ZONE_TRANSFER, (_T("CControlPipe::ProcessShortTransfer: jumping to status stage\n")));
        
        // this was the data stage of the transfer; fast-forward to the 
        // status stage
        m_pPipeQH->HW_paVLink = 
            GetTDPhysAddr( vaLastTD ) | QUEUE_ELEMENT_LINK_POINTER_TD | QUEUE_ELEMENT_LINK_POINTER_VALID;

        // pick up the transfer in the status stage
        fTransferInProgress = TRUE;
    } else {
        DEBUGMSG(ZONE_TRANSFER|ZONE_WARNING, (_T("CControlPipe::ProcessShortTransfer: unexpectedly short control transfer!\n")));
        // do normal cleanup (which always indicates that we're done with the current transfer)
        fTransferInProgress = CQueuedPipe::ProcessShortTransfer(pTD);
    }

    return fTransferInProgress;
}

// ******************************************************************               
// Scope: public
CInterruptPipe::CInterruptPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                                IN const BOOL fIsLowSpeed,
                                IN CUhcd *const pCUhcd)
//
// Purpose: Constructor for CInterruptPipe
//
// Parameters: See CQueuedPipe::CQueuedPipe
//
// Returns: Nothing
//
// Notes: Do not modify static variables here!!
// ******************************************************************
: CQueuedPipe( lpEndpointDescriptor, fIsLowSpeed, pCUhcd ) // constructor for base class
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("+CInterruptPipe::CInterruptPipe\n")) );
    DEBUGCHK( m_usbEndpointDescriptor.bDescriptorType == USB_ENDPOINT_DESCRIPTOR_TYPE &&
              m_usbEndpointDescriptor.bLength >= sizeof( USB_ENDPOINT_DESCRIPTOR ) &&
              (m_usbEndpointDescriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_INTERRUPT );

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("-CInterruptPipe::CInterruptPipe\n")) );
}

// ******************************************************************               
// Scope: public
CInterruptPipe::~CInterruptPipe( )
//
// Purpose: Destructor for CInterruptPipe
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: Do not modify static variables here!!
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("+CInterruptPipe::~CInterruptPipe\n")) );
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("-CInterruptPipe::~CInterruptPipe\n")) );
}

// ******************************************************************               
// Scope: public (Implements CPipe::OpenPipe = 0)
HCD_REQUEST_STATUS CInterruptPipe::OpenPipe( void )
//
// Purpose: Create the data structures necessary to conduct
//          transfers on this pipe
//
// Parameters: None
//
// Returns: requestOK - if pipe opened
//
//          requestFailed - if pipe was not opened
//
// Notes: 
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE, (TEXT("+CInterruptPipe::OpenPipe\n") ) );

    HCD_REQUEST_STATUS status = requestFailed;

    EnterCriticalSection( &m_csPipeLock );

    // if this fails, someone is trying to open
    // an already opened pipe
    DEBUGCHK( m_pPipeQH == NULL );
    if ( m_pPipeQH == NULL && 
         m_pUHCIFrame->m_pCPhysMem->AllocateMemory( 
                            DEBUG_PARAM( TEXT("Interrupt QH") )
                            sizeof( UHCD_QH ),
                            (PUCHAR*)&m_pPipeQH,
                            GetMemoryAllocationFlags() ) ) {
    #ifdef DEBUG
        m_pUHCIFrame->m_debug_QHMemoryAllocated += sizeof( UHCD_QH );
        DEBUGMSG( ZONE_QH, (TEXT("CInterruptPipe::OpenPipe - alloc 1 QH, total bytes = %d\n"), m_pUHCIFrame->m_debug_QHMemoryAllocated) );
    #endif // DEBUG

        // this is set to FALSE in the contructor
        DEBUGCHK( !m_fIsReclamationPipe );

        // Interrupt QHs are a bit complicated. See comment
        // in Initialize() routine as well.
        //
        // Essentially, we need to change the poll interval to be
        // a power of 2 which is <= UHCD_MAX_INTERRUPT_INTERVAL,
        // and then find a good spot in the m_pUHCIFrame->m_interruptQHTree to
        // insert the new QH
        //
        // first, find a new pollinterval
        {
            if ( m_usbEndpointDescriptor.bInterval >= 1 ) {
                UCHAR newInterval = UHCD_MAX_INTERRUPT_INTERVAL;
                while ( newInterval > m_usbEndpointDescriptor.bInterval ) {
                    newInterval >>= 1;
                }
            #ifdef DEBUG
                if ( newInterval != m_usbEndpointDescriptor.bInterval ) {
                    DEBUGCHK( newInterval >= 1 &&
                              newInterval < m_usbEndpointDescriptor.bInterval &&
                              UHCD_MAX_INTERRUPT_INTERVAL % newInterval == 0 );
                    DEBUGMSG( ZONE_WARNING, (TEXT("CInterruptPipe::OpenPipe - setting the poll interval to %d ms instead of %d ms\n"), newInterval, m_usbEndpointDescriptor.bInterval ));
                }
            #endif // DEBUG
                m_usbEndpointDescriptor.bInterval = newInterval;
            } else {
                DEBUGCHK( 0 );// is there REALLY a device with an interrupt
                              // endpoint of poll interval 0??? Or, has there
                              // been an error retrieving descriptors
                DEBUGMSG( ZONE_WARNING, (TEXT("CInterruptPipe::OpenPipe - warning! Interrupt pipe has poll interval 0. Changing to 1\n")) );
                m_usbEndpointDescriptor.bInterval = 1;
            }
        }

        EnterCriticalSection( &m_pUHCIFrame->m_csQHScheduleLock );

        // now find where to insert the QH
        // Given a poll interval of 2^k, we can insert the QH
        // into the interrupt tree after any of the QHs
        // 2^k, 2^k + 1, ... , 2^(k+1) - 1
        UCHAR insertAfter = m_usbEndpointDescriptor.bInterval;
        {
            // see if there is a better place to insert the QH
            UCHAR index = m_usbEndpointDescriptor.bInterval + 1;
            while ( m_pUHCIFrame->m_interruptQHTree[ insertAfter ]->dwInterruptTree.Load > 0 && index < 2 * m_usbEndpointDescriptor.bInterval ) {
                if ( m_pUHCIFrame->m_interruptQHTree[ index ]->dwInterruptTree.Load < m_pUHCIFrame->m_interruptQHTree[ insertAfter ]->dwInterruptTree.Load ) {
                    insertAfter = index;
                }
                index++;
            }
            DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("CInterruptPipe::OpenPipe - inserting interrupt QH after m_pUHCIFrame->m_interruptQHTree[ %d ], load count = %d\n"), insertAfter, m_pUHCIFrame->m_interruptQHTree[ insertAfter ]->dwInterruptTree.Load ) );
        }

        InitializeQH( m_pPipeQH, // QH to initialize
                      m_pUHCIFrame->m_interruptQHTree[ insertAfter ], // previous QH
                      m_pUHCIFrame->m_interruptQHTree[ insertAfter ]->HW_paHLink, // HW link to next QH
                      m_pUHCIFrame->m_interruptQHTree[ insertAfter ]->vaNextQH ); // next QH

        m_pPipeQH->dwInterruptTree.BranchIndex = insertAfter;

        // Next QH's prev is now our QH
        m_pUHCIFrame->m_interruptQHTree[ insertAfter ]->vaNextQH->vaPrevQH = m_pPipeQH;
        // Previous QH's next is now our QH
        m_pUHCIFrame->m_interruptQHTree[ insertAfter ]->vaNextQH = m_pPipeQH;
        // Insert ourselves into the schedule
        m_pUHCIFrame->m_interruptQHTree[ insertAfter ]->HW_paHLink = GetQHPhysAddr( m_pPipeQH )
                                                            | QUEUE_HEAD_LINK_POINTER_QH
                                                            | QUEUE_HEAD_LINK_POINTER_VALID;

        UpdateInterruptQHTreeLoad( (UCHAR) m_pPipeQH->dwInterruptTree.BranchIndex, 1 );

        LeaveCriticalSection( &m_pUHCIFrame->m_csQHScheduleLock );

        status = requestOK;
    }
    LeaveCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_PIPE, (TEXT("-CInterruptPipe::OpenPipe, returning HCD_REQUEST_STATUS %d\n"), status) );
    return status;
}

// ******************************************************************               
// Scope: private (over-rides CQueuedPipe::UpdateInterruptQHTreeLoad)
void CInterruptPipe::UpdateInterruptQHTreeLoad( IN const UCHAR branch,
                                                IN const int   deltaLoad )
//
// Purpose: Change the load counts of all members in m_pUHCIFrame->m_interruptQHTree
//          which are on branch "branch" by deltaLoad. This needs
//          to be done when interrupt QHs are added/removed to be able
//          to find optimal places to insert new queues
//
// Parameters: See CQueuedPipe::UpdateInterruptQHTreeLoad
//
// Returns: Nothing
//
// Notes:
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("+CInterruptPipe::UpdateInterruptQHTreeLoad - branch = %d, deltaLoad = %d\n"), branch, deltaLoad) );

    DEBUGCHK( branch >= 1 && branch < 2 * UHCD_MAX_INTERRUPT_INTERVAL );

    // first step - need to find the greatest power of 2 which is
    // <= branch
    UCHAR pow = UHCD_MAX_INTERRUPT_INTERVAL;
    while ( pow > branch ) {
        pow >>= 1;
    }

    EnterCriticalSection( &m_pUHCIFrame->m_csQHScheduleLock );

    // In the reverse direction, any queue which will eventually
    // point to m_pUHCIFrame->m_interruptQHTree[ branch ] needs to get
    // its dwInterruptTree.Load incremented. These are the queues
    // branch + n * pow, n = 1, 2, 3, ...
    for ( UCHAR link = branch + pow; link < 2 * UHCD_MAX_INTERRUPT_INTERVAL; link += pow ) {
        DEBUGCHK( m_pUHCIFrame->m_interruptQHTree[ link ]->dwInterruptTree.Load <= m_pUHCIFrame->m_interruptQHTree[ branch ]->dwInterruptTree.Load );
        m_pUHCIFrame->m_interruptQHTree[ link ]->dwInterruptTree.Load += deltaLoad;
    }
    // In the forward direction, any queue that 
    // m_pUHCIFrame->m_interruptQHTree[ branch ] eventually points to
    // needs its dwInterruptTree.Load incremented. These queues
    // are found using the same algorithm as CPipe::Initialize();
    link = branch;
    while ( link >= 1 ) {
        DEBUGCHK( ( link & pow ) &&
                  ( (link ^ pow) | (pow / 2) ) < link );
        DEBUGCHK( link == branch ||
                  m_pUHCIFrame->m_interruptQHTree[ link ]->dwInterruptTree.Load + deltaLoad >= m_pUHCIFrame->m_interruptQHTree[ branch ]->dwInterruptTree.Load );
        m_pUHCIFrame->m_interruptQHTree[ link ]->dwInterruptTree.Load += deltaLoad;
        link ^= pow;
        pow >>= 1;
        link |= pow;
    }

    LeaveCriticalSection( &m_pUHCIFrame->m_csQHScheduleLock );

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("-CInterruptPipe::UpdateInterruptQHTreeLoad - branch = %d, deltaLoad = %d\n"), branch, deltaLoad) );
}

// ******************************************************************               
// Scope: private (Implements CPipe::GetNumTDsNeeded = 0)
USHORT CInterruptPipe::GetNumTDsNeeded( const STransfer *pTransfer ) const 
//
// Purpose: Determine how many transfer descriptors (TDs) are needed
//          for an Interrupt transfer of the given size
//
// Parameters: None
//
// Returns: USHORT representing # of TDs needed
//
// Notes: Assumes m_csPipeLock held
// ******************************************************************
{
    if (pTransfer == NULL)
        pTransfer = &m_transfer;
    
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("+CInterruptPipe::GetNumTDsNeeded\n")) );

    DEBUGCHK( AreTransferParametersValid() );

    // Interrupt transfer will send wMaxPacketSize of data each TD, until
    // the last TD, where less data (but > 0) can be sent

    USHORT tdsNeeded = USHORT( pTransfer->dwBufferSize / m_usbEndpointDescriptor.wMaxPacketSize );
    if ( pTransfer->dwBufferSize % m_usbEndpointDescriptor.wMaxPacketSize != 0 ) {
        tdsNeeded++;
    }
    if (tdsNeeded == 0) {
        DEBUGCHK( pTransfer->dwBufferSize == 0 );
        tdsNeeded = 1;
    }

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("-CInterruptPipe::GetNumTDsNeeded, returning %d\n"), tdsNeeded ) );
    return tdsNeeded;
}

// ******************************************************************               
// Scope: private (Implements CPipe::AreTransferParametersValid = 0)
BOOL CInterruptPipe::AreTransferParametersValid( const STransfer *pTransfer ) const
//
// Purpose: Check whether this class' transfer parameters are valid.
//          This includes checking m_transfer, m_pPipeQH, etc
//
// Parameters: None (all parameters are vars of class)
//
// Returns: TRUE if parameters valid, else FALSE
//
// Notes: Assumes m_csPipeLock already held
// ******************************************************************
{
    if (pTransfer == NULL)
        pTransfer = &m_transfer;
    
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("+CInterruptPipe::AreTransferParametersValid\n")) );

    // these parameters aren't used by CInterruptPipe, so if they are non NULL,
    // it doesn't present a serious problem. But, they shouldn't have been
    // passed in as non-NULL by the calling driver.
    DEBUGCHK( pTransfer->adwIsochErrors == NULL && // ISOCH
              pTransfer->adwIsochLengths == NULL && // ISOCH
              pTransfer->aLengths == NULL && // ISOCH
              pTransfer->lpvControlHeader == NULL ); // CONTROL
    // this is also not a serious problem, but shouldn't happen in normal
    // circumstances. It would indicate a logic error in the calling driver.
    DEBUGCHK( !(pTransfer->lpfnCallback == NULL && pTransfer->lpvCallbackParameter != NULL) );
    // DWORD                     pTransfer->dwStartingFrame (ignored - ISOCH)
    // DWORD                     pTransfer->dwFrames (ignored - ISOCH)

    BOOL fValid = ( m_pPipeQH != NULL &&
                    pTransfer->address > 0 &&
                    pTransfer->address <= USB_MAX_ADDRESS &&
                    (pTransfer->lpvClientBuffer != NULL || pTransfer->dwBufferSize == 0) &&
                    // paClientBuffer could be 0 or !0
                    pTransfer->lpfComplete != NULL &&
                    pTransfer->lpdwBytesTransferred != NULL &&
                    pTransfer->lpdwError != NULL );

    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("-CInterruptPipe::AreTransferParametersValid, returning BOOL %d\n"), fValid) );
    return fValid;
}

// ******************************************************************               
// Scope: private (Implements CPipe::ScheduleTransfer = 0)
HCD_REQUEST_STATUS CInterruptPipe::ScheduleTransfer( void ) 
//
// Purpose: Schedule a USB Transfer on this pipe
//
// Parameters: None (all parameters are in m_transfer)
//
// Returns: requestOK if transfer issued ok, else requestFailed
//
// Notes: 
// ******************************************************************
{
    HCD_REQUEST_STATUS status = requestFailed;

    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("+CInterruptPipe::ScheduleTransfer\n")) );
    
    EnterCriticalSection( &m_csPipeLock );
    
    // this should already have been checked by CPipe::IssueTransfer
    DEBUGCHK( AreTransferParametersValid() );

    // specific to CInterruptPipe
    DEBUGCHK( (m_fTransferInProgress || m_pPipeQH->vaVertTD == NULL) &&
              (m_pPipeQH->HW_paVLink & QUEUE_ELEMENT_LINK_POINTER_TERMINATE) &&
              m_transfer.vaTDList != NULL &&
              ((m_transfer.vaActualBuffer != NULL && m_transfer.paActualBuffer != 0) ||
               m_transfer.dwBufferSize == 0) &&
              m_transfer.numTDsInList > 0 );
    
    DEBUGMSG(ZONE_TRANSFER, (TEXT("CInterruptPipe::ScheduleTransfer dwFlags(0x%X), cancelId=0x%X\n"),
                             m_transfer.dwFlags, m_transfer.lpvCancelId));

    // make sure data toggles are still correct
    PUHCD_TD pTD = (PUHCD_TD) m_transfer.vaTDList;
    if (pTD->DataToggle == m_dataToggleC) {
        // we were unlucky; adjust the TD queue
        DEBUGMSG(ZONE_TRANSFER & ZONE_VERBOSE, (TEXT("CInterruptPipe::ScheduleTransfer adjusting data toggles\n")) );
        for (int i=0; i<m_transfer.numTDsInList; ++i)
            (pTD++)->DataToggle ^= 1;
    }

    // insert into schedule (FALSE = add to end of list)
    if ( m_fTransferInProgress || m_pUHCIFrame->AddToBusyPipeList( this, FALSE ) ) {
        m_pPipeQH->vaVertTD = PUHCD_TD( m_transfer.vaTDList );
        m_pPipeQH->HW_paVLink =
            GetTDPhysAddr( m_pPipeQH->vaVertTD ) | QUEUE_ELEMENT_LINK_POINTER_TD | QUEUE_ELEMENT_LINK_POINTER_VALID;

        DEBUGCHK( !m_fIsReclamationPipe );
        m_fTransferInProgress = TRUE;
        status = requestOK;
    }
    
    LeaveCriticalSection( &m_csPipeLock );
    
    DEBUGMSG( ZONE_WARNING && status != requestOK, (TEXT("CInterruptPipe::ScheduleTransfer - unable to issue transfer!!!\n")) );
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("-CInterruptPipe::ScheduleTransfer, returning HCD_REQUEST_STATUS %d\n"), status) );
    return status;

}

// ******************************************************************               
// Scope: private
HCD_REQUEST_STATUS CInterruptPipe::AddTransfer( STransfer *pTransfer ) 
//
// Purpose: Schedule a 2nd (or 3rd or ...) USB Transfer on this pipe
//
// Parameters: an STransfer structure (which contains the transfer parameters)
//
// Returns: requestOK if transfer issued ok, else requestFailed
//
// Notes: 
// ******************************************************************
{
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("+CInterruptPipe::AddTransfer\n")) );
    
    EnterCriticalSection( &m_csPipeLock );
    
    // this should already have been checked by CPipe::IssueTransfer
    DEBUGCHK( AreTransferParametersValid() );

    // specific to CInterruptPipe
    DEBUGCHK( pTransfer->vaTDList != NULL &&
              ((pTransfer->vaActualBuffer != NULL &&
                pTransfer->paActualBuffer != 0) ||
               pTransfer->dwBufferSize == 0) &&
              pTransfer->numTDsInList > 0 );
    
    const DWORD         dwDataPID = (pTransfer->dwFlags & USB_IN_TRANSFER) ?
                                        TD_IN_PID : TD_OUT_PID;
    const UCHAR         endpoint = UCHAR( m_usbEndpointDescriptor.bEndpointAddress & TD_ENDPOINT_MASK );
    DWORD               dwTDOffset = 0;
    DWORD               dwBufferOffset = 0;
    USHORT              tdNumber = 0;
    BOOL                bShortPacketDetect = !!(pTransfer->dwFlags & USB_IN_TRANSFER);

    DEBUGMSG(ZONE_TRANSFER, (L"CInterruptPipe::AddTransfer pTransfer->dwFlags(%X),packetsize=%X\n",pTransfer->dwFlags,m_usbEndpointDescriptor.wMaxPacketSize));
    
    while ( ++tdNumber < pTransfer->numTDsInList ) {
        PUHCD_TD vaThisTD = PUHCD_TD( pTransfer->vaTDList + dwTDOffset );
        PUHCD_TD vaNextTD = PUHCD_TD( pTransfer->vaTDList + dwTDOffset + sizeof( UHCD_TD ) );
    
        DEBUGCHK( dwBufferOffset + m_usbEndpointDescriptor.wMaxPacketSize < pTransfer->dwBufferSize );
    
        InitializeTD( vaThisTD, // TD to initialize
                      GetTDPhysAddr( vaNextTD )
                             | TD_LINK_POINTER_VALID
                             | TD_LINK_POINTER_TD
                             | TD_LINK_POINTER_BREADTH_FIRST, // HW Link
                      vaNextTD, // virt addr of next TD
                      0, // InterruptOnComplete,
                      0, // Isochronous,
                      m_fIsLowSpeed, // LowSpeedControl,
                      dwDataPID, // PID (IN/OUT/SETUP)
                      pTransfer->address, // Device Address,
                      endpoint, // Device endpoint
                      (m_dataToggleQ ^= 1), // Data Toggle (USB spec 8.5.1)
                      m_usbEndpointDescriptor.wMaxPacketSize - 1, // MaxLength (n-1) form
                      pTransfer->paActualBuffer + dwBufferOffset , // phys addr of buffer
                      bShortPacketDetect);  
    
        dwTDOffset += sizeof( UHCD_TD );
        dwBufferOffset += m_usbEndpointDescriptor.wMaxPacketSize;
    }
    DEBUGCHK( tdNumber == pTransfer->numTDsInList );
    DEBUGCHK( pTransfer->dwBufferSize - dwBufferOffset >= 0 &&
              pTransfer->dwBufferSize - dwBufferOffset <= m_usbEndpointDescriptor.wMaxPacketSize );
    const PUHCD_TD vaLastTD = PUHCD_TD( pTransfer->vaTDList + dwTDOffset );
    InitializeTD( vaLastTD, // TD to initialize
                  TD_LINK_POINTER_TERMINATE, // HW link
                  NULL, // virt addr of next TD
                  1, // InterruptOnComplete,
                  0, // Isochronous,
                  m_fIsLowSpeed, // LowSpeedControl,
                  dwDataPID, // PID (IN/OUT/SETUP)
                  pTransfer->address, // Device Address,
                  endpoint, // Device endpoint
                  (m_dataToggleQ ^= 1), // Data Toggle (USB spec 8.5.1)
                  pTransfer->dwBufferSize - dwBufferOffset - 1, // MaxLength (n-1) form
                  pTransfer->paActualBuffer + dwBufferOffset , // phys addr of buffer
                  bShortPacketDetect);  

    LeaveCriticalSection( &m_csPipeLock );
    
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("-CInterruptPipe::ScheduleTransfer, returning success\n")) );
    return requestOK;
}

// ******************************************************************               
// Scope: public
CIsochronousPipe::CIsochronousPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                                    IN const BOOL fIsLowSpeed, 
                                    IN CUhcd *const pCUhcd )
//
// Purpose: Constructor for CIsochronousPipe
//
// Parameters: See CPipe::CPipe
//
// Returns: Nothing
//
// Notes: Do not modify static variables here!!
// ******************************************************************
: CPipe( lpEndpointDescriptor, fIsLowSpeed, pCUhcd) // constructor for base class
, m_pCUhcd(pCUhcd)
, m_pWakeupTD( NULL ) // TD used to wake us up for transfers scheduled in future
, m_fUsingWakeupTD( FALSE ) // wakeup TD is not in use yet
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("+CIsochronousPipe::CIsochronousPipe\n")) );
    DEBUGCHK( m_usbEndpointDescriptor.bDescriptorType == USB_ENDPOINT_DESCRIPTOR_TYPE &&
              m_usbEndpointDescriptor.bLength >= sizeof( USB_ENDPOINT_DESCRIPTOR ) &&
              (m_usbEndpointDescriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_ISOCHRONOUS );

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("-CIsochronousPipe::CIsochronousPipe\n")) );
}

// ******************************************************************               
// Scope: public
CIsochronousPipe::~CIsochronousPipe( )
//
// Purpose: Destructor for CIsochronousPipe
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: Do not modify static variables here!!
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("+CIsochronousPipe::~CIsochronousPipe\n")) );
    // m_pWakeupTD should have been freed by the time we get here
    DEBUGCHK( m_pWakeupTD == NULL );
    DEBUGCHK( !m_fUsingWakeupTD );
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("-CIsochronousPipe::~CIsochronousPipe\n")) );
}

// ******************************************************************               
// Scope: public (Implements CPipe::OpenPipe = 0)
HCD_REQUEST_STATUS CIsochronousPipe::OpenPipe( void )
//
// Purpose: Inserting the necessary (empty) items into the
//          schedule to permit future transfers
//
// Parameters: None
//
// Returns: requestOK if pipe opened successfuly
//          requestFailed if pipe not opened
//
// Notes: 
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE, (TEXT("+CIsochronousPipe::OpenPipe\n")) );

    HCD_REQUEST_STATUS status = requestFailed;

    EnterCriticalSection( &m_csPipeLock );

    DEBUGCHK( m_usbEndpointDescriptor.bDescriptorType == USB_ENDPOINT_DESCRIPTOR_TYPE &&
              m_usbEndpointDescriptor.bLength >= sizeof( USB_ENDPOINT_DESCRIPTOR ) &&
              (m_usbEndpointDescriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_ISOCHRONOUS );

    // if this fails, someone is trying to open an already opened pipe
    DEBUGCHK( m_pWakeupTD == NULL );
    if ( m_pWakeupTD == NULL &&
         m_pUHCIFrame->m_pCPhysMem->AllocateMemory( DEBUG_PARAM( TEXT("Isoch Wakeup TD") )
                                      sizeof( UHCD_TD ),
                                      (PUCHAR*)&m_pWakeupTD,
                                      CPHYSMEM_FLAG_NOBLOCK ) ) { // don't waste high-pri mem on this
        #ifdef DEBUG
            m_pUHCIFrame->m_debug_TDMemoryAllocated += sizeof( UHCD_TD );
            DEBUGMSG( ZONE_TD, (TEXT("CIsochronousPipe::OpenPipe - create Isoch Wakeup TD, total bytes = %d\n"), m_pUHCIFrame->m_debug_TDMemoryAllocated) );
        #endif // DEBUG

            // ok, got TD
            InitializeTD( m_pWakeupTD, // TD to init
                          TD_LINK_POINTER_TERMINATE, // HW link to next TD
                          NULL, // virtual address of next TD
                          1, // Interrupt on complete
                          1, // Isochronous
                          m_fIsLowSpeed, // low speed control
                          TD_IN_PID, // PID (must be valid, even though unused)
                          0, // address (0-127, even though unused)
                          0, // endpoint (0-15, even though unused)
                          0, // data toggle (always 0 for Isoch)
                          TD_MAXLENGTH_NULL_BUFFER, // size of data buffer
                          0 ); // physical address of buffer
            // InitializeTD sets Active to 1
            DEBUGCHK( m_pWakeupTD->Active == 1 );
            m_pWakeupTD->Active = 0;

            status = requestOK;
    }
    DEBUGCHK( !m_fUsingWakeupTD );
    LeaveCriticalSection( &m_csPipeLock );
    DEBUGMSG( ZONE_PIPE, (TEXT("-CIsochronousPipe::OpenPipe, returning HCD_REQUEST_STATUS %d\n"), status ) );
    return status;
}

// ******************************************************************               
// Scope: public (Implements CPipe::ClosePipe = 0)
HCD_REQUEST_STATUS CIsochronousPipe::ClosePipe( void )
//
// Purpose: Abort any transfers associated with this pipe, and
//          remove its data structures from the schedule
//
// Parameters: None
//
// Returns: requestOK
//
// Notes: 
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE, (TEXT("+CIsochronousPipe::ClosePipe\n")) );

    // Don't enter CS before calling AbortTransfer, since
    // that function will need to leave the pipe CS
    // at a certain point to avoid deadlock, and
    // won't be able to do so if the CS is nested.

    while (m_fTransferInProgress)
        AbortTransfer( NULL,
                       NULL,
                       m_transfer.lpvCancelId );

    EnterCriticalSection( &m_csPipeLock );

    // if this fails, a transfer is in progress, which
    // shouldn't happen considering we aborted transfer
    // above.
    DEBUGCHK( !m_fTransferInProgress );
    DEBUGCHK( !m_fUsingWakeupTD );

    // if this fails, someone is trying to close an unopened pipe.
    DEBUGCHK( m_pWakeupTD != NULL );
    if ( m_pWakeupTD != NULL ) {
        // if this fails, m_pWakeupTD was not properly disabled
        // the last time a transfer completed/aborted
        DEBUGCHK( m_pWakeupTD->Active == 0 &&
                  m_pWakeupTD->vaNextTD == NULL &&
                  m_pWakeupTD->vaPrevIsochTD == NULL );

        m_pUHCIFrame->m_pCPhysMem->FreeMemory( PUCHAR( m_pWakeupTD ),
                                 GetTDPhysAddr( m_pWakeupTD ),
                                 CPHYSMEM_FLAG_NOBLOCK );
        m_pWakeupTD = NULL;
    #ifdef DEBUG
        m_pUHCIFrame->m_debug_TDMemoryAllocated -= sizeof( UHCD_TD );
        DEBUGMSG( ZONE_TD, (TEXT("CIsochronousPipe::ClosePipe - delete Isoch Wakeup TD, total bytes = %d\n"), m_pUHCIFrame->m_debug_TDMemoryAllocated) );
    #endif // DEBUG
    }

    LeaveCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_PIPE, (TEXT("-CIsochronousPipe::ClosePipe\n")) );
    return requestOK;
}


// ******************************************************************               
// Scope: public (Implements CPipe::AbortTransfer = 0)
HCD_REQUEST_STATUS CIsochronousPipe::AbortTransfer( 
                                    IN const LPTRANSFER_NOTIFY_ROUTINE lpCancelAddress,
                                    IN const LPVOID lpvNotifyParameter,
                                    IN LPCVOID lpvCancelId )
//
// Purpose: Abort any transfer on this pipe if its cancel ID matches
//          that which is passed in.
//
// Parameters: lpCancelAddress - routine to callback after aborting transfer
//
//             lpvNotifyParameter - parameter for lpCancelAddress callback
//
//             lpvCancelId - identifier for transfer to abort
//
// Returns: requestOK if transfer aborted
//          requestFailed if lpvCancelId doesn't match currently executing
//                 transfer, or if there is no transfer in progress
//
// Notes:
// ******************************************************************
{
    DEBUGMSG( ZONE_TRANSFER, (TEXT("+CIsochronousPipe::AbortTransfer\n")));

    HCD_REQUEST_STATUS status = requestFailed;
    BOOL               fLeftCS = FALSE;
    STransfer        * pTransfer;

    EnterCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_PIPE && !m_fTransferInProgress,
              (TEXT("CIsochronousPipe::AbortTransfer - no transfer is in progress\n")) );

    if (m_fTransferInProgress) {
        for (pTransfer = &m_transfer;
             pTransfer != NULL && pTransfer->lpvCancelId != lpvCancelId;
             pTransfer = pTransfer->lpNextTransfer)
            if (pTransfer->lpNextTransfer == m_pLastTransfer)
                // if anything gets removed, it'll be the last one in the list;
                // otherwise, we'll need to reset this later.
                m_pLastTransfer = pTransfer;
        if (pTransfer == NULL && m_pLastTransfer->lpNextTransfer != NULL)
            // oops - didn't find one, so reset my state
            m_pLastTransfer = m_pLastTransfer->lpNextTransfer;
    }

    if ( m_fTransferInProgress && pTransfer != NULL ) {
        DEBUGCHK( pTransfer->lpvCancelId == lpvCancelId );

        EnterCriticalSection( &m_pUHCIFrame->m_csFrameListLock );

        DEBUGCHK( pTransfer->dwFrames == DWORD( pTransfer->numTDsInList ) );
        DWORD dwTDOffset = 0;
        for ( DWORD dwFrameOffset = 0; dwFrameOffset < pTransfer->dwFrames; dwFrameOffset++ ) {
            PUHCD_TD pTD = (PUHCD_TD) (pTransfer->vaTDList + dwTDOffset);

            DEBUGCHK( pTD->Isochronous == 1 &&
                      pTD->LowSpeedControl == (DWORD) m_fIsLowSpeed &&
                      pTD->Endpoint == (DWORD) (m_usbEndpointDescriptor.bEndpointAddress & TD_ENDPOINT_MASK) &&
                      pTD->Address == (DWORD) pTransfer->address );

            pTD->Active = 0;
            m_pUHCIFrame->RemoveIsochTDFromFrameList( pTD,
                                        (pTransfer->dwStartingFrame + dwFrameOffset) & FRAME_LIST_LENGTH_MASK );

            dwTDOffset += sizeof( UHCD_TD );
        }
        
        // remove the wakeup TD only if there are no other transfers pending on this pipe
        if ( m_fUsingWakeupTD && m_transfer.lpNextTransfer == NULL ) {
            m_pUHCIFrame->RemoveIsochTDFromFrameList( m_pWakeupTD,
                                        (pTransfer->dwStartingFrame - ISOCH_TD_WAKEUP_INTERVAL) & FRAME_LIST_LENGTH_MASK );
            m_fUsingWakeupTD = FALSE;
        }

        LeaveCriticalSection( &m_pUHCIFrame->m_csFrameListLock );

        // Now we need to ensure that the hardware is no longer accessing
        // the TDs which made up this transfer. In the future, perhaps
        // this can be done on a separate thread.
        m_pCUhcd->CHW::WaitOneFrame();

        DWORD dwOldPermissions =  SetProcPermissions(m_transfer.dwCurrentPermissions);
#pragma prefast(disable: 322, "Recover gracefully from hardware failure")
        __try { // accessing caller's memory  
            *pTransfer->lpdwError = USB_CANCELED_ERROR;
            *pTransfer->lpfComplete = TRUE;
            *pTransfer->lpdwBytesTransferred = 0;
        } __except( EXCEPTION_EXECUTE_HANDLER ) {
        }
#pragma prefast(pop)
        SetProcPermissions(dwOldPermissions);

        // cache the callback info because *pTransfer will be transmogrified
        LPTRANSFER_NOTIFY_ROUTINE lpfnCompletion = pTransfer->lpfnCallback;
        LPVOID                    lpvParam = pTransfer->lpvCallbackParameter;

        STransfer *pNext = pTransfer->lpNextTransfer;
        if (pNext == m_pLastTransfer)
            m_pLastTransfer = pTransfer;
        FreeTransferMemory(pTransfer);
        if (pNext) {
            *pTransfer = *pNext; // yes, yes, it's a copy.
            delete pNext;
        } else {
            if (m_pLastTransfer == pTransfer) {
                // this is the only transfer in the queue
                m_fTransferInProgress = FALSE;
                m_pLastTransfer = NULL;
            } else {
                DEBUGCHK( m_pLastTransfer->lpNextTransfer == pTransfer );
                m_pLastTransfer->lpNextTransfer = NULL;
            }
        }
        // from this point on, pTransfer is no longer what it was.

        // need to exit crit sec after checking the flag but before
        // calling Remove to avoid deadlock with CheckForDoneTransfersThread
        if ( m_fTransferInProgress )
        {
            LeaveCriticalSection( &m_csPipeLock );
            fLeftCS = TRUE;
        } else {
            // the queue is now empty so the pipe is no longer busy
            DEBUGCHK( m_pLastTransfer == NULL );
            LeaveCriticalSection( &m_csPipeLock );
            fLeftCS = TRUE;
            m_pUHCIFrame->RemoveFromBusyPipeList( this );
        }
    
        // ok, transfer has been aborted. Perform callback if needed.
        if ( lpfnCompletion ) {
            __try { // calling the transfer complete callback
                ( *lpfnCompletion )( lpvParam );
            } __except( EXCEPTION_EXECUTE_HANDLER ) {
                  DEBUGMSG( ZONE_ERROR, (TEXT("CIsochronousPipe::AbortTransfer - exception executing completion callback function\n")) );
            }
        }
        if ( lpCancelAddress ) {
            __try { // calling the Cancel function
                ( *lpCancelAddress )( lpvNotifyParameter );
            } __except( EXCEPTION_EXECUTE_HANDLER ) {
                  DEBUGMSG( ZONE_ERROR, (TEXT("CIsochronousPipe::AbortTransfer - exception executing cancellation callback function\n")) );
            }
        }
        status = requestOK;
    }
    if ( !fLeftCS ) {
        LeaveCriticalSection( &m_csPipeLock );
    }

    DEBUGMSG( ZONE_TRANSFER, (TEXT("-CIsochronousPipe::AbortTransfer\n")));
    return status;
}

// ******************************************************************               
// Scope: private (Implements CPipe::GetNumTDsNeeded = 0)
USHORT CIsochronousPipe::GetNumTDsNeeded( const STransfer *pTransfer ) const 
//
// Purpose: Determine how many transfer descriptors (TDs) are needed
//          for an Isochronous transfer of the given size
//
// Parameters: None
//
// Returns: USHORT representing # of TDs needed
//
// Notes: Assumes m_csPipeLock held
// ******************************************************************
{
    if (pTransfer == NULL)
        pTransfer = &m_transfer;
    
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("+CIsochronousPipe::GetNumTDsNeeded\n")) );

    // For Isochronous transfers, we are already passed in the # of frames
    // over which to conduct the transfer, which is how many TDs are needed.

    DEBUGCHK( AreTransferParametersValid() );

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("-CIsochronousPipe::GetNumTDsNeeded, returning %d\n"), pTransfer->dwFrames  ) );
    return USHORT( pTransfer->dwFrames );
}

// ******************************************************************               
// Scope: private (Implements CPipe::AreTransferParametersValid = 0)
BOOL CIsochronousPipe::AreTransferParametersValid( const STransfer *pTransfer ) const
//
// Purpose: Check whether this class' transfer parameters, stored in
//          m_transfer, are valid.
//
// Parameters: None (all parameters are vars of class)
//
// Returns: TRUE if parameters valid, else FALSE
//
// Notes: Assumes m_csPipeLock already held
// ******************************************************************
{
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("+CIsochronousPipe::AreTransferParametersValid\n")) );

    if (pTransfer == NULL)
        pTransfer = &m_transfer;

    // these parameters aren't used by CIsochronousPipe, so if they are non NULL,
    // it doesn't present a serious problem. But, they shouldn't have been
    // passed in as non-NULL by the calling driver.
    DEBUGCHK( pTransfer->lpvControlHeader == NULL ); // CONTROL
    // this is also not a serious problem, but shouldn't happen in normal
    // circumstances. It would indicate a logic error in the calling driver.
    DEBUGCHK( !(pTransfer->lpfnCallback == NULL && pTransfer->lpvCallbackParameter != NULL) );

    // if this fails, a transfer is being issued on an unopened pipe
    DEBUGCHK( m_pWakeupTD != NULL );
    BOOL fValid = ( m_pWakeupTD != NULL &&
                    pTransfer->address > 0 &&
                    pTransfer->address <= USB_MAX_ADDRESS &&
                    pTransfer->lpvClientBuffer != NULL &&
                    // paClientBuffer could be 0 or !0
                    pTransfer->dwBufferSize > 0 &&
                    pTransfer->adwIsochErrors != NULL &&
                    pTransfer->adwIsochLengths != NULL &&
                    pTransfer->aLengths != NULL &&
                    pTransfer->dwFrames > 0 &&
                    pTransfer->dwFrames < FRAME_LIST_LENGTH - ISOCH_TD_WAKEUP_INTERVAL &&
                    pTransfer->lpfComplete != NULL &&
                    pTransfer->lpdwBytesTransferred != NULL &&
                    pTransfer->lpdwError != NULL );

    if ( fValid ) {
        DWORD dwOldPermissions =  SetProcPermissions(m_transfer.dwCurrentPermissions);
        __try {
            DWORD dwTotalData = 0;
            for ( DWORD frame = 0; frame < pTransfer->dwFrames; frame++ ) {
                if ( pTransfer->aLengths[ frame ] == 0 || 
                     pTransfer->aLengths[ frame ] > m_usbEndpointDescriptor.wMaxPacketSize ) {
                    fValid = FALSE;
                    break;
                }
                dwTotalData += pTransfer->aLengths[ frame ];
            }
            fValid = ( fValid &&
                       dwTotalData == pTransfer->dwBufferSize );
        } __except( EXCEPTION_EXECUTE_HANDLER ) {
            fValid = FALSE;
        }
        SetProcPermissions(dwOldPermissions);
    }

    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("-CIsochronousPipe::AreTransferParametersValid, returning BOOL %d\n"), fValid) );
    return fValid;
}

// ******************************************************************               
// Scope: private (over-rides CPipe::GetMemoryAllocationFlags)
DWORD CIsochronousPipe::GetMemoryAllocationFlags( void ) const
//
// Purpose: Get flags for allocating memory from the CPhysMem class.
//
// Parameters: None
//
// Returns: DWORD representing memory allocation flags
//
// Notes: 
// ******************************************************************
{
    return CPHYSMEM_FLAG_HIGHPRIORITY | CPHYSMEM_FLAG_NOBLOCK;
}

// ******************************************************************               
// Scope: private (Implements CPipe::ScheduleTransfer = 0)
HCD_REQUEST_STATUS CIsochronousPipe::ScheduleTransfer( void ) 
//
// Purpose: Schedule a USB Transfer on this pipe
//
// Parameters: None (all parameters are in m_transfer)
//
// Returns: requestOK if transfer issued ok, else requestFailed
//
// Notes: 
// ******************************************************************
{
    DEBUGMSG( ZONE_TRANSFER, (TEXT("+CIsochronousPipe::ScheduleTransfer\n")) );

    HCD_REQUEST_STATUS status = requestFailed;

    EnterCriticalSection( &m_csPipeLock );

    if ( m_pUHCIFrame->AddToBusyPipeList( this, TRUE ) )  // TRUE = add to start of list
    {
        m_fTransferInProgress = TRUE;
        status = requestOK;
    }

    LeaveCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_TRANSFER, (TEXT("-CIsochronousPipe::ScheduleTransfer, returning HCD_REQUEST_STATUS %d\n"), status) );
    return status;
}

// ******************************************************************               
// Scope: private
HCD_REQUEST_STATUS CIsochronousPipe::AddTransfer( STransfer *pTransfer ) 
//
// Purpose: Schedule a 2nd (or 3rd or ...) USB Transfer on this pipe
//
// Parameters: an STransfer structure (which contains the transfer parameters)
//
// Returns: requestOK if transfer issued ok, else requestFailed
//
// Notes: 
// ******************************************************************
{
    DEBUGMSG( ZONE_TRANSFER, (TEXT("+CIsochronousPipe::AddTransfer\n")) );

    HCD_REQUEST_STATUS status = requestFailed;

    EnterCriticalSection( &m_csPipeLock );
    
    DWORD dwCurrentFrameNumber;
    m_pCUhcd->CHW::GetFrameNumber(&dwCurrentFrameNumber);

    if (pTransfer->dwFlags & USB_START_ISOCH_ASAP)
        if (m_fTransferInProgress)
            pTransfer->dwStartingFrame = m_pLastTransfer->dwStartingFrame + m_pLastTransfer->dwFrames;
        else
            // the frame number could have just rolled over,
            // so add 1ms. Then, add another 2ms to account for
            // delay in scheduling this transfer
            pTransfer->dwStartingFrame = dwCurrentFrameNumber + 3;

    if ( pTransfer->dwStartingFrame < dwCurrentFrameNumber + 3) {
        SetLastError(ERROR_CAN_NOT_COMPLETE);
        DEBUGMSG( ZONE_TRANSFER|ZONE_WARNING,
                  (TEXT("!CIsochronousPipe::AddTransfer - cannot meet the schedule\n")) );
        goto done;
    }

    if (m_fTransferInProgress) {
        if (pTransfer->dwStartingFrame != m_pLastTransfer->dwStartingFrame + m_pLastTransfer->dwFrames)
            // the new transfer must stream seamlessly on the last one or things can become tricky
            goto done;
        if (m_transfer.dwStartingFrame + ISOCH_STREAMING_MAX <= pTransfer->dwStartingFrame + pTransfer->dwFrames)
            // the total number of queued frames must be less than a fixed max to keep this simple
            goto done;
    }
    
    {   // this brace prevents the compiler from whining about the above goto's. really.
    EnterCriticalSection( &m_pUHCIFrame->m_csFrameListLock );
    DEBUGCHK( m_fTransferInProgress || !m_fUsingWakeupTD );

    // from this point on, we can't fail
    const UCHAR endpoint = UCHAR( m_usbEndpointDescriptor.bEndpointAddress & TD_ENDPOINT_MASK );
    const DWORD dwDataPID = (pTransfer->dwFlags & USB_IN_TRANSFER) ? TD_IN_PID : TD_OUT_PID;

    if ( pTransfer->dwStartingFrame + pTransfer->dwFrames > dwCurrentFrameNumber + FRAME_LIST_LENGTH ) {
        // this should only happen if client is specifying start frame
        DEBUGCHK( !(pTransfer->dwFlags & USB_START_ISOCH_ASAP) );
        // this transfer is too far into the future - need delay
        DEBUGCHK( !m_fTransferInProgress &&     // otherwise this case would be caught above
                  !m_fUsingWakeupTD &&          // can't be in use when no xfrs in progress
                  m_pWakeupTD->vaNextTD == NULL &&
                  m_pWakeupTD->vaPrevIsochTD == NULL );

        m_fUsingWakeupTD = TRUE;

        DWORD dwWakeupFrame = (pTransfer->dwStartingFrame - ISOCH_TD_WAKEUP_INTERVAL) & FRAME_LIST_LENGTH_MASK;

        InitializeTD( m_pWakeupTD, // TD to init
                      TD_LINK_POINTER_TERMINATE, // HW link - will be set correctly below
                      NULL, // next TD - will be set correctly below
                      1, // Interrupt on Complete
                      1, // Isochronous
                      m_fIsLowSpeed, // low speed control
                      dwDataPID, // PID - must be valid, even though unused
                      pTransfer->address, // address - must be valid, even though unused
                      endpoint, // endpoint - must be valid, even though unused
                      0, // data toggle
                      TD_MAXLENGTH_NULL_BUFFER, // MaxLength - must be valid, even though unused
                      0 ); // physical address of buffer

        DEBUGCHK( m_pWakeupTD->Active == 1 );
        m_pUHCIFrame->InsertIsochTDIntoFrameList( m_pWakeupTD, dwWakeupFrame );
    }

    DWORD       dwTDOffset = 0;
    DWORD       dwBufferOffset = 0;

    // ok, now we're going to start putting TDs into the schedule
    DEBUGCHK( pTransfer->dwFrames == DWORD( pTransfer->numTDsInList ) );
    for ( DWORD dwFrameOffset = 0; dwFrameOffset < pTransfer->dwFrames; dwFrameOffset++ ) {
        // frameIndex is the actual position in the frame list where
        // this frame's TD will be scheduled
        const DWORD frameIndex = (pTransfer->dwStartingFrame + dwFrameOffset) & FRAME_LIST_LENGTH_MASK;
        PUHCD_TD    vaThisTD = PUHCD_TD( pTransfer->vaTDList + dwTDOffset );

        // should already have been checked in AreTransferParametersValid
        DEBUGCHK( pTransfer->aLengths[ dwFrameOffset ] > 0 &&
                  pTransfer->aLengths[ dwFrameOffset ] <= m_usbEndpointDescriptor.wMaxPacketSize );

        InitializeTD( vaThisTD, // TD to initialize
                      TD_LINK_POINTER_TERMINATE, // HW link - set correctly below
                      NULL, // virt addr of next TD - set correctly below
                      (dwFrameOffset == pTransfer->dwFrames - 1), // InterruptOnComplete,
                      1, // Isochronous,
                      m_fIsLowSpeed, // LowSpeedControl,
                      dwDataPID, // PID (IN/OUT)
                      pTransfer->address, // Device Address,
                      endpoint, // Device endpoint
                      0, // Data Toggle (USB spec 8.5.4)
                      pTransfer->aLengths[ dwFrameOffset ] - 1, // MaxLength (n-1) form
                      pTransfer->paActualBuffer + dwBufferOffset ); // phys addr of buffer

        DEBUGCHK( vaThisTD->Active == 1 );
        if ( m_fUsingWakeupTD ) {
            vaThisTD->Active = 0;
        }

        m_pUHCIFrame->InsertIsochTDIntoFrameList( vaThisTD, frameIndex );

        dwTDOffset += sizeof( UHCD_TD );
        dwBufferOffset += pTransfer->aLengths[ dwFrameOffset ];
    }
    DEBUGCHK( dwBufferOffset == pTransfer->dwBufferSize );
    status = requestOK;

    LeaveCriticalSection( &m_pUHCIFrame->m_csFrameListLock );
    }

  done:
    LeaveCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_TRANSFER, (TEXT("-CIsochronousPipe::AddTransfer\n")) );
    return status;
}

// ******************************************************************               
// Scope: private (Implements CPipe::CheckForDoneTransfers = 0)
BOOL CIsochronousPipe::CheckForDoneTransfers( void )
//
// Purpose: Check if the transfer on this pipe is finished, and 
//          take the appropriate actions - i.e. remove the transfer
//          data structures and call any notify routines
//
// Parameters: None
//
// Returns: TRUE if this pipe is no longer busy; FALSE if there are still
//          some pending transfers.
//
// Notes:
// ******************************************************************
{
    DEBUGMSG( ZONE_TRANSFER, (TEXT("+CIsochronousPipe::CheckForDoneTransfers\n")) );

    BOOL fTransferDone = FALSE;
    EnterCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_TRANSFER && !m_fTransferInProgress, (TEXT("CIsochronousPipe::CheckForDoneTransfers - transfer was already finished on entry\n") ) );
    if ( m_fTransferInProgress ) {
    #ifdef DEBUG
        DWORD dwOldPermissions1 =  SetProcPermissions(m_transfer.dwCurrentPermissions);
        __try { // checking if all parameters are as we expect...
                // (need a __try because of dereference of m_transfer.xyz params)
            DEBUGCHK( *m_transfer.lpdwBytesTransferred == 0 &&
                      *m_transfer.lpfComplete == FALSE &&
                      *m_transfer.lpdwError == USB_NOT_COMPLETE_ERROR );
        } __except( EXCEPTION_EXECUTE_HANDLER ) {
              // this shouldn't happen
        }
        SetProcPermissions(dwOldPermissions1);
    #endif // DEBUG

        if ( m_fUsingWakeupTD ) {
            DEBUGCHK( !(m_pWakeupTD->HW_paLink & TD_LINK_POINTER_TERMINATE) );
            DEBUGCHK( !(m_transfer.dwFlags & USB_START_ISOCH_ASAP) );
            DEBUGCHK( m_pWakeupTD->Isochronous == 1 &&
                      m_pWakeupTD->InterruptOnComplete == 1 ); 

            DWORD dwCurrentFrameNumber;
            m_pCUhcd->CHW::GetFrameNumber(&dwCurrentFrameNumber);
            DEBUGCHK( dwCurrentFrameNumber < m_transfer.dwStartingFrame );

            if ( m_transfer.dwStartingFrame + m_transfer.dwFrames < dwCurrentFrameNumber + FRAME_LIST_LENGTH ) {
                // we can now activate the transfer
                PUHCD_TD pTD;
                DWORD    dwTDOffset = 0;
                for ( DWORD dwFrameOffset = 0; dwFrameOffset < m_transfer.dwFrames; dwFrameOffset++ ) {
                    pTD = PUHCD_TD( m_transfer.vaTDList + dwTDOffset );
#if 0
                    DEBUGCHK( pTD->Active == 0 &&
                              pTD->Isochronous == 1 &&
                              pTD->LowSpeedControl == (DWORD) m_fIsLowSpeed &&
                              pTD->Endpoint == (DWORD) (m_usbEndpointDescriptor.bEndpointAddress & TD_ENDPOINT_MASK) &&
                              pTD->Address == (DWORD) m_transfer.address );
#else // above DEBUGCHK is getting hit, and I can't figure out why,
      // because in the debugger all fields match up!! Try breaking
      // check into multiple parts to figure out which part fails.
                    DEBUGCHK( pTD->Active == 0 );
                    DEBUGCHK( pTD->Isochronous == 1 );
                    DEBUGCHK( pTD->LowSpeedControl == (DWORD) m_fIsLowSpeed );
                    DEBUGCHK( pTD->Endpoint == (DWORD) (m_usbEndpointDescriptor.bEndpointAddress & TD_ENDPOINT_MASK ));
                    DEBUGCHK( pTD->Address == (DWORD) m_transfer.address );
#endif
                    pTD->Active = 1;
                    dwTDOffset += sizeof( UHCD_TD );
                }

                EnterCriticalSection( &m_pUHCIFrame->m_csFrameListLock );
                m_pWakeupTD->InterruptOnComplete = 0;
                m_pUHCIFrame->RemoveIsochTDFromFrameList( m_pWakeupTD,
                                            (m_transfer.dwStartingFrame - ISOCH_TD_WAKEUP_INTERVAL) & FRAME_LIST_LENGTH_MASK );
                m_fUsingWakeupTD = FALSE;
                LeaveCriticalSection( &m_pUHCIFrame->m_csFrameListLock );

            }
        } else {
            PUHCD_TD pLastTD = PUHCD_TD( m_transfer.vaTDList + (m_transfer.numTDsInList - 1) * sizeof( UHCD_TD ) );
            if ( pLastTD->Active == 0 ) {
                // turn off further interrupts on this transfer
                DEBUGCHK( pLastTD->InterruptOnComplete == 1 );
                pLastTD->InterruptOnComplete = 0;

                EnterCriticalSection( &m_pUHCIFrame->m_csFrameListLock );

                DWORD dwTDOffset = 0;
                DWORD dwOverallError = USB_NO_ERROR;
                DWORD dwOverallLength = 0;

                DEBUGCHK( m_transfer.dwFrames == (DWORD) m_transfer.numTDsInList );
                for ( DWORD dwFrameOffset = 0; dwFrameOffset < m_transfer.dwFrames; dwFrameOffset++ ) {
                    PUHCD_TD pTD = PUHCD_TD( m_transfer.vaTDList + dwTDOffset );
#if 0
                    DEBUGCHK( pTD->Active == 0 &&
                              pTD->Isochronous == 1 &&
                              pTD->LowSpeedControl == (DWORD) m_fIsLowSpeed &&
                              pTD->Endpoint == (DWORD) (m_usbEndpointDescriptor.bEndpointAddress & TD_ENDPOINT_MASK) &&
                              pTD->Address == (DWORD) m_transfer.address );
#else // above DEBUGCHK is getting hit, and I can't figure out why,
      // because in the debugger all fields match up!! Try breaking
      // check into multiple parts to figure out which part fails.
                    //DEBUGCHK( pTD->Active == 0 );
                    DEBUGCHK( pTD->Isochronous == 1 );
                    DEBUGCHK( pTD->LowSpeedControl == (DWORD) m_fIsLowSpeed );
                    DEBUGCHK( pTD->Endpoint == (DWORD) (m_usbEndpointDescriptor.bEndpointAddress & TD_ENDPOINT_MASK ));
                    DEBUGCHK( pTD->Address == (DWORD) m_transfer.address );
#endif
                    DWORD dwError = USB_NO_ERROR;
                    DWORD dwLength = UNENC_LEN(pTD->ActualLength);

                    if ( pTD->StatusField & TD_STATUS_STALLED ) {
                        DEBUGMSG( ZONE_WARNING, (TEXT("CIsochronousPipe::CheckForDoneTransfers - failure on TD 0x%x, dwFrameOffset = %d, address = %d, endpoint = %d, status field = 0x%x\n"), pTD, dwFrameOffset, pTD->Address, pTD->Endpoint, pTD->StatusField ) );
                        dwOverallError = USB_STALL_ERROR;
                        dwError = USB_STALL_ERROR;
                        dwLength = 0;
                        m_fIsHalted = TRUE;
                    } else if ( pTD->StatusField != TD_STATUS_NO_ERROR ) {
//                        DEBUGMSG( ZONE_WARNING, (TEXT("CIsochronousPipe::CheckForDoneTransfers - failure on TD 0x%x, dwFrameOffset = %d, address = %d, endpoint = %d, status field = 0x%x\n"), pTD, dwFrameOffset, pTD->Address, pTD->Endpoint, pTD->StatusField ) );
                        dwError = USB_NOT_COMPLETE_ERROR;
                        dwLength = 0;
                    }
                    dwOverallLength += dwLength;

                    DWORD dwOldPermissions =  SetProcPermissions(m_transfer.dwCurrentPermissions);
#pragma prefast(disable: 322, "Recover gracefully from hardware failure")
                    __try { // setting isoch OUT status parameters
                        m_transfer.adwIsochErrors[ dwFrameOffset ] = dwError;
                        m_transfer.adwIsochLengths[ dwFrameOffset ] = dwLength;
                    } __except( EXCEPTION_EXECUTE_HANDLER ) {
                    }
#pragma prefast(pop)
                    SetProcPermissions(dwOldPermissions);

                    m_pUHCIFrame->RemoveIsochTDFromFrameList( pTD,
                                                (m_transfer.dwStartingFrame + dwFrameOffset) & FRAME_LIST_LENGTH_MASK );

                    dwTDOffset += sizeof( UHCD_TD );
                }
                DEBUGCHK( dwTDOffset == m_transfer.numTDsInList * sizeof( UHCD_TD ) );
                LeaveCriticalSection( &m_pUHCIFrame->m_csFrameListLock );

                // IN transfer where client did not specify a buffer
                // for us to DMA into. Thus, we created our own internal
                // buffer, and now need to copy the data.
                if ( (m_transfer.dwFlags & USB_IN_TRANSFER) &&
                     m_transfer.paClientBuffer == 0 ) {
                    DEBUGCHK( m_transfer.dwBufferSize > 0 &&
                              m_transfer.lpvClientBuffer != NULL &&
                              m_transfer.paActualBuffer != 0 &&
                              m_transfer.vaActualBuffer != NULL );

                    DWORD dwOldPermissions =  SetProcPermissions(m_transfer.dwCurrentPermissions);
                    __try {
                        memcpy( PUCHAR( m_transfer.lpvClientBuffer ), m_transfer.vaActualBuffer, m_transfer.dwBufferSize );
                    } __except( EXCEPTION_EXECUTE_HANDLER ) {
                          dwOverallLength = 0;
                          dwOverallError = USB_CLIENT_BUFFER_ERROR;
                    }
                    SetProcPermissions(dwOldPermissions);
                }

                // the TDs/Buffers associated with the transfer
                // can now be freed
                FreeTransferMemory();

                DWORD dwOldPermissions =  SetProcPermissions(m_transfer.dwCurrentPermissions);
#pragma prefast(disable: 322, "Recover gracefully from hardware failure")
                __try { // setting transfer status and executing callback function
                    *m_transfer.lpfComplete = TRUE;
                    *m_transfer.lpdwError = dwOverallError;
                    *m_transfer.lpdwBytesTransferred = dwOverallLength;
                    if ( m_transfer.lpfnCallback ) {
                        ( *m_transfer.lpfnCallback )( m_transfer.lpvCallbackParameter );
                    }
                } __except( EXCEPTION_EXECUTE_HANDLER ) {
                }
#pragma prefast(pop)
                SetProcPermissions(dwOldPermissions);

                // this transfer is now officially done; are more pending?
                STransfer *pNext = m_transfer.lpNextTransfer;
                if (pNext) {
                    DEBUGCHK(m_pLastTransfer != NULL);
                    if (pNext == m_pLastTransfer)
                        m_pLastTransfer = &m_transfer;
                    m_transfer = *pNext;    // copy for historical reasons; will fix someday.
                    delete pNext;
                } else {
                    // ok, all transfers are done
                    DEBUGCHK(m_pLastTransfer == &m_transfer);
                    m_fTransferInProgress = FALSE;
                    m_pLastTransfer = NULL;
                    fTransferDone = TRUE;
                #ifdef DEBUG
                    memset( &m_transfer, GARBAGE, sizeof( m_transfer ) );
                #endif // DEBUG
                }
            }
        }
    } else
        // There was nothing to do and we successfully did it;
        // don't bother us anymore.
        fTransferDone = TRUE;
    
    LeaveCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_TRANSFER, (TEXT("-CIsochronousPipe::CheckForDoneTransfers, returning BOOL %d\n"), fTransferDone) );
    return fTransferDone;
}
CPipeAbs * CreateBulkPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
               IN const BOOL fIsLowSpeed,
               IN const UCHAR /*bDeviceAddress*/,
               IN CHcd * const pChcd)
{ 
    return new CBulkPipe(lpEndpointDescriptor,fIsLowSpeed,(CUhcd * const)pChcd);
}
CPipeAbs * CreateControlPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                  IN const BOOL fIsLowSpeed,
                  IN const UCHAR /*bDeviceAddress*/,
                  IN CHcd * const pChcd)
{ 
    return new CControlPipe(lpEndpointDescriptor,fIsLowSpeed,(CUhcd * const)pChcd);
}

CPipeAbs * CreateInterruptPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
               IN const BOOL fIsLowSpeed,
               IN const UCHAR /*bDeviceAddress*/,
               IN CHcd * const pChcd)
{ 
    return new CInterruptPipe(lpEndpointDescriptor,fIsLowSpeed,(CUhcd * const)pChcd);
}

CPipeAbs * CreateIsochronousPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
               IN const BOOL fIsLowSpeed,
               IN const UCHAR /*bDeviceAddress*/,
               IN CHcd * const pChcd)
{ 
    return new CIsochronousPipe(lpEndpointDescriptor,fIsLowSpeed,(CUhcd * const)pChcd);
}



