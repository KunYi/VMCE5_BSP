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
//     CPipe.hpp
// 
// Abstract: Implements class for managing open pipes for UHCI
//
//                             CPipe (ADT)
//                           /             \
//                  CQueuedPipe (ADT)       CIsochronousPipe
//                /         |       \ 
//              /           |         \
//   CControlPipe    CInterruptPipe    CBulkPipe
// 
// Notes: 
// 

#ifndef __CPIPE_HPP__
#define __CPIPE_HPP__

#include <globals.hpp>
#include <cphysmem.hpp>
#include <pipeabs.hpp>

class CPipe;
class CIsochronousPipe;
class CQueuedPipe;
class CControlPipe;
class CInterruptPipe;
class CBulkPipe;
class CUHCIFrame;
class CUhcd;
struct _UHCD_TD;
struct _UHCD_QH;

typedef enum { TYPE_UNKNOWN =0, TYPE_CONTROL, TYPE_BULK, TYPE_INTERRUPT, TYPE_ISOCHRONOUS } PIPE_TYPE;
//
// Frame List (Section 3.1 of UHCI spec)
//
// number of entries in the frame list - 1024 
#define FRAME_LIST_LENGTH                   DWORD(0x400)
#define FRAME_LIST_LENGTH_MASK              DWORD(0x3FF)
// each of the 1024 entries of the frame list is 32 bits
typedef ULONG FRAME_LIST_POINTER;
// this should come out to be 4Kb, and will be debug checked in the code
#define FRAME_LIST_SIZE_IN_BYTES            DWORD(FRAME_LIST_LENGTH * sizeof(FRAME_LIST_POINTER))
// UHCI spec section 3.1.1 defines the structure of the Frame List Pointer
#define FRAME_LIST_POINTER_MASK             DWORD(0xFFFFFFF0)
#define FRAME_LIST_POINTER_TERMINATE        DWORD(1 << 0)
#define FRAME_LIST_POINTER_VALID            DWORD(0 << 0)
#define FRAME_LIST_POINTER_QH               DWORD(1 << 1)
#define FRAME_LIST_POINTER_TD               DWORD(0 << 1)

//
// Transfer Descriptor for UHCI (Section 3.2 of UHCI spec)
//
typedef ULONG TD_LINK_POINTER_PHYSICAL_ADDRESS;
#define TD_LINK_POINTER_MASK                DWORD(0xFFFFFFF0)
#define TD_LINK_POINTER_TERMINATE           DWORD(1 << 0)
#define TD_LINK_POINTER_VALID               DWORD(0 << 0)
#define TD_LINK_POINTER_QH                  DWORD(1 << 1)
#define TD_LINK_POINTER_TD                  DWORD(0 << 1)
#define TD_LINK_POINTER_DEPTH_FIRST         DWORD(1 << 2)
#define TD_LINK_POINTER_BREADTH_FIRST       DWORD(0 << 2)
typedef ULONG TD_BUFFER_PHYSICAL_ADDRESS;
typedef ULONG TD_PHYSICAL_ADDRESS;

typedef struct _UHCD_TD * PUHCD_TD;
typedef struct _UHCD_TD {
    TD_LINK_POINTER_PHYSICAL_ADDRESS HW_paLink;     // DWORD1 - link used by host controller
                                                    // to find next TD/QH to process
    DWORD    ActualLength:11;         // DWORD2, 0 ..10 - actual amount of data transferred
                                      //                  encoded in (n-1) form
    DWORD    Reserved_1:6;            // DWORD2, 11..16
    DWORD    StatusField:6;           // DWORD2, 17..22 - used to indicate done transfer's status
    DWORD    Active:1;                // DWORD2, 23 - indicates whether transfer is active
    DWORD    InterruptOnComplete:1;   // DWORD2, 24 - indicates to send USB interrupt when finished
    DWORD    Isochronous:1;           // DWORD2, 25 - indicates Isochronous vs Queued transfer
    DWORD    LowSpeedControl:1;       // DWORD2, 26 - indicates transfer to low speed device
    DWORD    ErrorCounter:2;          // DWORD2, 27..28 - this field is decremented every time
                                      //                  there is an error on the transfer
    DWORD    ShortPacketDetect:1;     // DWORD2, 29 - indicates to allow ActualLength < MaxLength
    DWORD    ReservedMBZ:2;           // DWORD2, 30..31
    
    DWORD    PID:8;                   // DWORD3, 0..7 - indicates SETUP/IN/OUT transfer
    DWORD    Address:7;               // DWORD3, 8..14 - address of device to send transfer to
    DWORD    Endpoint:4;              // DWORD3, 15..18 - endpoint on device to send transfer to
    DWORD    DataToggle:1;            // DWORD3, 19 - used to send multipacket transfers
    DWORD    Reserved_2:1;            // DWORD3, 20
    DWORD    MaxLength:11;            // DWORD3, 21..31 - maximum data size to send/receive
    TD_BUFFER_PHYSICAL_ADDRESS      HW_paBuffer;    // DWORD4 - phys addr of data buffer

    // These 4 DWORDs are for software use
    PUHCD_TD                            vaPrevIsochTD;  // prev TD (only for Isoch)
    PUHCD_TD                            vaNextTD;       // next TD
    DWORD                               dwUNUSED1;      // unused for now
    DWORD                               dwUNUSED2;      // unused for now
} UHCD_TD;
// Status bits for StatusField in UHCD_TD
// Reserved bit is taken care of by Reserved_1 above
#define TD_STATUS_NO_ERROR                  DWORD(0)
#define TD_STATUS_BITSTUFF_ERROR            DWORD(1 << 0)
#define TD_STATUS_CRC_TIMEOUT_ERROR         DWORD(1 << 1)
#define TD_STATUS_NAK_RECEIVED              DWORD(1 << 2)
#define TD_STATUS_BABBLE_DETECTED           DWORD(1 << 3)
#define TD_STATUS_DATA_BUFFER_ERROR         DWORD(1 << 4)
#define TD_STATUS_STALLED                   DWORD(1 << 5)
#define TD_STATUS_EVERY_ERROR               DWORD(TD_STATUS_BITSTUFF_ERROR | TD_STATUS_CRC_TIMEOUT_ERROR | TD_STATUS_NAK_RECEIVED | TD_STATUS_BABBLE_DETECTED | TD_STATUS_DATA_BUFFER_ERROR | TD_STATUS_STALLED)
// For ErrorCounter field
#define TD_ERRORCOUNTER_NEVER_INTERRUPT              DWORD(0)
#define TD_ERRORCOUNTER_INTERRUPT_AFTER_ONE          DWORD(1)
#define TD_ERRORCOUNTER_INTERRUPT_AFTER_TWO          DWORD(2)
#define TD_ERRORCOUNTER_INTERRUPT_AFTER_THREE        DWORD(3)
// Active bit has its own field above
// TDs are 32 bytes long
#define TD_REQUIRED_SIZE_IN_BYTES           DWORD(32)
// TDs must be aligned on 16 byte boundaries
#define TD_ALIGNMENT_BOUNDARY               DWORD(16)
#define TD_ENDPOINT_MASK                    DWORD(0xF)
// constants for MaxLength field
#define TD_MAXLENGTH_MAX                    DWORD(0x4FF)
#define TD_MAXLENGTH_INVALID                DWORD(0x7FE)
#define TD_MAXLENGTH_NULL_BUFFER            DWORD(0x7FF)
// constants for ActualLength field (used for SW to maintain data integrity)
#define TD_ACTUALLENGTH_INVALID             TD_MAXLENGTH_INVALID
// constants for the PID (Packet Identifcation) field
// see UHCI spec 3.2.3
#define TD_IN_PID                           DWORD(0x69)
#define TD_OUT_PID                          DWORD(0xE1)
#define TD_SETUP_PID                        DWORD(0x2D)

// Queue Head for UHCI (Section 3.3 of UHCI spec)
typedef ULONG QUEUE_HEAD_LINK_POINTER_PHYSICAL_ADDRESS;
#define QUEUE_HEAD_LINK_POINTER_MASK         DWORD(0xFFFFFFF0)
#define QUEUE_HEAD_LINK_POINTER_TERMINATE    DWORD(1 << 0)
#define QUEUE_HEAD_LINK_POINTER_VALID        DWORD(0 << 0)
#define QUEUE_HEAD_LINK_POINTER_QH           DWORD(1 << 1)
// #define QUEUE_HEAD_LINK_POINTER_TD        DWORD(0 << 1) <- our QH's never point horizontally to TDs
typedef ULONG QUEUE_ELEMENT_LINK_POINTER_PHYSICAL_ADDRESS; 
#define QUEUE_ELEMENT_LINK_POINTER_MASK      DWORD(0xFFFFFFF0)
#define QUEUE_ELEMENT_LINK_POINTER_TERMINATE DWORD(1 << 0)
#define QUEUE_ELEMENT_LINK_POINTER_VALID     DWORD(0 << 0)
// #define QUEUE_ELEMENT_LINK_POINTER_QH     DWORD(1 << 1) <- our QH's never point vertically to QHs
#define QUEUE_ELEMENT_LINK_POINTER_TD        DWORD(0 << 1)

typedef struct _UHCD_QH * PUHCD_QH;
typedef struct _UHCD_QH {
    QUEUE_HEAD_LINK_POINTER_PHYSICAL_ADDRESS    HW_paHLink; // phys addr of next QH
    QUEUE_ELEMENT_LINK_POINTER_PHYSICAL_ADDRESS HW_paVLink; // phys addr of queued TD
    
    // queue heads must be aligned on 16 byte boundaries. We'll make
    // them 32 bytes long. These fields are for SW use only.

    PUHCD_QH                                    vaPrevQH;       // virt addr of prev QH
    PUHCD_QH                                    vaNextQH;       // virt addr of next QH
    PUHCD_TD                                    vaVertTD;       // virt addr of queued TD

    // dwInterruptTree is used for interrupt transfer QHs.
    // For m_interruptQHTree members, which are just placeholders and 
    // do not actually carry transfers, the Load field will describe
    // how much interrupt traffic follows the QH branch. For other 
    // QHs, the BranchIndex field will describe where in the tree the
    // QH is located

    union {
        DWORD                                   Load;
        DWORD                                   BranchIndex;
    }                                           dwInterruptTree;

    DWORD                                       dwUNUSED1;      // unused...
    DWORD                                       dwUNUSED2;      // unused...
} UHCD_QH;
// QHs must be aligned on 16 byte boundaries
#define QH_ALIGNMENT_BOUNDARY               DWORD(16)

// THIS ***MUST*** BE A POWER OF TWO!!! It is the maximum number of milliseconds
// that can go between polling for an interrupt on a device. We use this
// to set up the interrupt queue tree. This tree contains 2*MAX_INTERRUPT_INTERVAL - 1
// nodes, and allows us to specify intervals of 1, 2, 4, 8, ..., MAX_INTERRUPT_INTERVAL
#define UHCD_MAX_INTERRUPT_INTERVAL UCHAR(32)

// structure used for managing busy pipes
typedef struct _PIPE_LIST_ELEMENT {
    CPipe*                      pPipe;
    struct _PIPE_LIST_ELEMENT * pNext;
} PIPE_LIST_ELEMENT, *PPIPE_LIST_ELEMENT;

struct STransfer {
    // These are the IssueTransfer parameters
    UCHAR                     address;
    LPTRANSFER_NOTIFY_ROUTINE lpfnCallback;
    LPVOID                    lpvCallbackParameter;
    DWORD                     dwFlags;
    LPCVOID                   lpvControlHeader;
    DWORD                     paControlHeader;
    DWORD                     dwStartingFrame;
    DWORD                     dwFrames;
    LPCDWORD                  aLengths;
    DWORD                     dwBufferSize;     
    LPVOID                    lpvClientBuffer;
    ULONG                     paClientBuffer;
    LPCVOID                   lpvCancelId;
    LPDWORD                   adwIsochErrors;
    LPDWORD                   adwIsochLengths;
    LPBOOL                    lpfComplete;
    LPDWORD                   lpdwBytesTransferred;
    LPDWORD                   lpdwError;
    // additional parameters/data
    PUCHAR                    vaTDList;         // TD list for the transfer
    USHORT                    numTDsInList;     // # TDs in pTDListHead list
    PUCHAR                    vaActualBuffer;   // virt addr of buffer used by TD list
    ULONG                     paActualBuffer;   // phys addr of buffer used by TD list
    DWORD                     dwCurrentPermissions;
    struct STransfer *        lpNextTransfer;
};
void     InitializeTD( OUT PUHCD_TD const pTD,
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
                              IN const BOOL bShortPacketOk = FALSE);

void InitializeQH( OUT PUHCD_QH const pQH, 
                              IN const PUHCD_QH vaPrevQH,
                              IN const QUEUE_HEAD_LINK_POINTER_PHYSICAL_ADDRESS HW_paHLink,
                              IN const PUHCD_QH vaNextQH );

    // Return the index of the m_interruptQHTree that this frame should point to
inline UCHAR QHTreeEntry( IN const DWORD frame )
{
    DEBUGCHK( frame >= 0 && frame < FRAME_LIST_LENGTH );
    // return interval + (frame % interval)
    DEBUGCHK( frame % UHCD_MAX_INTERRUPT_INTERVAL == (frame & (UHCD_MAX_INTERRUPT_INTERVAL - 1)) );
    return UCHAR( UHCD_MAX_INTERRUPT_INTERVAL + (frame & (UHCD_MAX_INTERRUPT_INTERVAL - 1)) );
};
//----------------------------------------------------------------------------    
class CUHCIFrame {
    friend class CPipe;
    friend class CIsochronousPipe;
    friend class CQueuedPipe;
    friend class CControlPipe;
    friend class CInterruptPipe;
    friend class CBulkPipe;
public:
    CUHCIFrame(IN CPhysMem* const pCPhysMem );
    ~CUHCIFrame();
    BOOL Initialize( CUhcd * m_pCUhcd );

    void         DeInitialize( void );

    inline ULONG GetFrameListPhysAddr( void )
    {
        DEBUGCHK( m_vaFrameList != NULL );
        DEBUGCHK( m_pCPhysMem->VaToPa( PUCHAR( m_vaFrameList ) ) % FRAME_LIST_SIZE_IN_BYTES == 0 );
        return m_pCPhysMem->VaToPa( PUCHAR( m_vaFrameList ) );
    };
    inline ULONG GetQHPhysAddr( IN PUHCD_QH virtAddr ) {
        DEBUGCHK( virtAddr != NULL &&
                  m_pCPhysMem->VaToPa( PUCHAR(virtAddr) ) % QH_ALIGNMENT_BOUNDARY == 0 );
        return m_pCPhysMem->VaToPa( PUCHAR(virtAddr) );
    }
    inline ULONG GetTDPhysAddr( IN PUHCD_TD virtAddr ) {
        DEBUGCHK( virtAddr != NULL &&
                  m_pCPhysMem->VaToPa( PUCHAR(virtAddr) ) % TD_ALIGNMENT_BOUNDARY == 0 );
        return m_pCPhysMem->VaToPa( PUCHAR(virtAddr) );
    }


    void         SignalCheckForDoneTransfers( void );
    
    void     HandleReclamationLoadChange( IN const BOOL fAddingTransfer  );

private:
    // ****************************************************
    // Private Functions for CPipe
    // ****************************************************
    static ULONG CALLBACK CheckForDoneTransfersThreadStub( IN PVOID pContext);
    ULONG CheckForDoneTransfersThread();

    // ****************************************************
    // Private Variables for CPipe
    // ****************************************************
#ifdef DEBUG
    BOOL             m_debug_fInitializeAlreadyCalled;
#endif // DEBUG

    // CheckForDoneTransfersThread related variables
    BOOL             m_fCheckTransferThreadClosing; // signals CheckForDoneTransfersThread to exit
    HANDLE           m_hCheckForDoneTransfersEvent; // event for CheckForDoneTransfersThread
    HANDLE           m_hCheckForDoneTransfersThread; // thread for handling done transfers
    CRITICAL_SECTION m_csBusyPipeListLock;
    PPIPE_LIST_ELEMENT m_pBusyPipeList;
#ifdef DEBUG
    int              m_debug_numItemsOnBusyPipeList;
#endif // DEBUG
    UINT            numReclamationTransfers;
public:
    BOOL     AddToBusyPipeList( IN CPipe* const pPipe,
                                       IN const BOOL fHighPriority );

    void     RemoveFromBusyPipeList( IN CPipe* const pPipe );
    
    void     InsertIsochTDIntoFrameList( IN_OUT PUHCD_TD pTD,
                                                    IN const DWORD dwFrameIndex );
    void     RemoveIsochTDFromFrameList( IN_OUT PUHCD_TD pTD,
                                                    IN const DWORD dwFrameIndex );
    CPhysMem * const m_pCPhysMem;            // memory object for our TD/QH and Frame list alloc
    // ****************************************************
    // Protected Variables for CPipe
    // ****************************************************
private:
    CUhcd * m_pCUhcd;
    // schedule related vars
    CRITICAL_SECTION m_csFrameListLock;     // critical section for frame list
    PULONG           m_vaFrameList;          // virtual address of Frame List
    CRITICAL_SECTION m_csQHScheduleLock;    // crit sec for QH section of schedule
    PUHCD_QH         m_interruptQHTree[ 2 * UHCD_MAX_INTERRUPT_INTERVAL ]; 
                                            // array to keep track of the binary tree containing
                                            // the interrupt queue heads.
    PUHCD_QH         m_pFinalQH;      // final QH in the schedule
    PUHCD_TD         m_pFrameSynchTD; // for keeping the software frame counter in sync

#ifdef DEBUG
    int              m_debug_TDMemoryAllocated;
    int              m_debug_QHMemoryAllocated;
    int              m_debug_BufferMemoryAllocated;
    int              m_debug_ControlExtraMemoryAllocated;
#endif // DEBUG
};

class CPipe : public CPipeAbs
{
    friend class CUHCIFrame;  // Declare a friend class
public:
    // ****************************************************
    // Public Functions for CPipe
    // ****************************************************
    CPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
           IN const BOOL fIsLowSpeed,
           IN CUHCIFrame *const pUHCIFrame);

    virtual ~CPipe();

    virtual PIPE_TYPE GetType () { return TYPE_UNKNOWN; };

    virtual HCD_REQUEST_STATUS  OpenPipe( void ) = 0;

    virtual HCD_REQUEST_STATUS  ClosePipe( void ) = 0;

    HCD_REQUEST_STATUS  IssueTransfer( 
                                IN const UCHAR address,
                                IN LPTRANSFER_NOTIFY_ROUTINE const lpfnCallback,
                                IN LPVOID const lpvCallbackParameter,
                                IN const DWORD dwFlags,
                                IN LPCVOID const lpvControlHeader,
                                IN const DWORD dwStartingFrame,
                                IN const DWORD dwFrames,
                                IN LPCDWORD const aLengths,
                                IN const DWORD dwBufferSize,     
                                IN_OUT LPVOID const lpvBuffer,
                                IN const ULONG paBuffer,
                                IN LPCVOID const lpvCancelId,
                                OUT LPDWORD const adwIsochErrors,
                                OUT LPDWORD const adwIsochLengths,
                                OUT LPBOOL const lpfComplete,
                                OUT LPDWORD const lpdwBytesTransferred,
                                OUT LPDWORD const lpdwError );

    virtual HCD_REQUEST_STATUS AbortTransfer( 
                                IN const LPTRANSFER_NOTIFY_ROUTINE lpCancelAddress,
                                IN const LPVOID lpvNotifyParameter,
                                IN LPCVOID lpvCancelId ) = 0;

    HCD_REQUEST_STATUS IsPipeHalted( OUT LPBOOL const lpbHalted );

    virtual void ClearHaltedFlag( void );
    
    // ****************************************************
    // Public Variables for CPipe
    // ****************************************************
    UCHAR m_bEndpointAddress;

protected:
    CUHCIFrame * const m_pUHCIFrame;
    // ****************************************************
    // Protected Functions for CPipe
    // ****************************************************
#ifdef DEBUG
    virtual const TCHAR*  GetPipeType( void ) const = 0;
#endif // DEBUG

    virtual USHORT  GetNumTDsNeeded( const STransfer *pTransfer = NULL ) const = 0;

    virtual BOOL    AreTransferParametersValid( const STransfer *pTransfer = NULL ) const = 0;

    virtual BOOL    CheckForDoneTransfers( void ) = 0;

    virtual DWORD   GetMemoryAllocationFlags( void ) const;

    virtual HCD_REQUEST_STATUS  ScheduleTransfer( void ) = 0;

    virtual HCD_REQUEST_STATUS  AddTransfer( STransfer *pTransfer )
        // The default defn simply disallows the queueing of multiple transfers.
        { /* compiler wants this referenced: */ (void) pTransfer;
          return m_fTransferInProgress ? requestFailed : requestOK; };


    void            FreeTransferMemory( STransfer *pTransfer = NULL );


    inline ULONG GetTDPhysAddr( IN PUHCD_TD virtAddr ) {
        DEBUGCHK( virtAddr != NULL &&
                  m_pUHCIFrame->m_pCPhysMem->VaToPa( PUCHAR(virtAddr) ) % TD_ALIGNMENT_BOUNDARY == 0 );
        return m_pUHCIFrame->m_pCPhysMem->VaToPa( PUCHAR(virtAddr) );
    }

    inline ULONG GetQHPhysAddr( IN PUHCD_QH virtAddr ) {
        DEBUGCHK( virtAddr != NULL &&
                  m_pUHCIFrame->m_pCPhysMem->VaToPa( PUCHAR(virtAddr) ) % QH_ALIGNMENT_BOUNDARY == 0 );
        return m_pUHCIFrame->m_pCPhysMem->VaToPa( PUCHAR(virtAddr) );
    }

protected:

    // pipe specific variables
    CRITICAL_SECTION        m_csPipeLock;           // crit sec for this specific pipe's variables
    USB_ENDPOINT_DESCRIPTOR m_usbEndpointDescriptor; // descriptor for this pipe's endpoint
    BOOL                    m_fIsLowSpeed;          // indicates speed of this pipe
    BOOL                    m_fIsHalted;            // indicates pipe is halted
    BOOL                    m_fTransferInProgress;  // indicates if this pipe is currently executing a transfer
    // WARNING! These parameters are treated as a unit. They
    // can all be wiped out at once, for example when a 
    // transfer is aborted.
    STransfer               m_transfer;            // Parameters for transfer on pipe
    STransfer *             m_pLastTransfer;      // ptr to last transfer in queue
};

class CQueuedPipe : public CPipe
{

public:
    // ****************************************************
    // Public Functions for CQueuedPipe
    // ****************************************************
    CQueuedPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                 IN const BOOL fIsLowSpeed,
                 IN CUhcd *const pCUhcd);
    virtual ~CQueuedPipe();

    HCD_REQUEST_STATUS ClosePipe( void );

    HCD_REQUEST_STATUS AbortTransfer( 
                                IN const LPTRANSFER_NOTIFY_ROUTINE lpCancelAddress,
                                IN const LPVOID lpvNotifyParameter,
                                IN LPCVOID lpvCancelId );

    // ****************************************************
    // Public Variables for CQueuedPipe
    // ****************************************************
    virtual void ClearHaltedFlag( void );

private:
    // ****************************************************
    // Private Functions for CQueuedPipe
    // ****************************************************
    void  AbortQueue( void ); 

    // ****************************************************
    // Private Variables for CQueuedPipe
    // ****************************************************

protected:
    // ****************************************************
    // Protected Functions for CQueuedPipe
    // ****************************************************

    BOOL         CheckForDoneTransfers( void );

    virtual BOOL ProcessShortTransfer( PUHCD_TD pTD );

    virtual void UpdateInterruptQHTreeLoad( IN const UCHAR branch,
                                            IN const int   deltaLoad );

    // ****************************************************
    // Protected Variables for CQueuedPipe
    // ****************************************************
    BOOL         m_fIsReclamationPipe; // indicates if this pipe is participating in bandwidth reclamation
    PUHCD_QH     m_pPipeQH;            // queue head for transfer
    UCHAR        m_dataToggleC;        // Data toggle of last completed TD
    UCHAR        m_dataToggleQ;        // Data toggle for next queued TD

    CUhcd * const m_pCUhcd;
};

class CBulkPipe : public CQueuedPipe
{
public:
    // ****************************************************
    // Public Functions for CBulkPipe
    // ****************************************************
    CBulkPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
               IN const BOOL fIsLowSpeed,
               IN CUhcd *const pCUhcd);
    ~CBulkPipe();

    virtual PIPE_TYPE GetType () { return TYPE_BULK; };

    HCD_REQUEST_STATUS    OpenPipe( void );
    // ****************************************************
    // Public variables for CBulkPipe
    // ****************************************************


private:
    // ****************************************************
    // Private Functions for CBulkPipe
    // ****************************************************

#ifdef DEBUG
    const TCHAR*  GetPipeType( void ) const
    {
        static const TCHAR* cszPipeType = TEXT("Bulk");
        return cszPipeType;
    }
#endif // DEBUG

    USHORT              GetNumTDsNeeded( const STransfer *pTransfer = NULL ) const;

    BOOL                AreTransferParametersValid( const STransfer *pTransfer = NULL ) const;

    HCD_REQUEST_STATUS  ScheduleTransfer( void );

    HCD_REQUEST_STATUS  AddTransfer( STransfer *pTransfer );

    // ****************************************************
    // Private variables for CBulkPipe
    // ****************************************************
};

class CControlPipe : public CQueuedPipe
{
public:
    // ****************************************************
    // Public Functions for CControlPipe
    // ****************************************************
    CControlPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                  IN const BOOL fIsLowSpeed,
                  IN CUhcd *const pCUhcd);
    ~CControlPipe();

    virtual PIPE_TYPE GetType () { return TYPE_CONTROL; };

    HCD_REQUEST_STATUS  OpenPipe( void );

    void                ChangeMaxPacketSize( IN const USHORT wMaxPacketSize );

    // ****************************************************
    // Public variables for CControlPipe
    // ****************************************************

private:
    // ****************************************************
    // Private Functions for CControlPipe
    // ****************************************************

#ifdef DEBUG
    const TCHAR*  GetPipeType( void ) const
    {
        static const TCHAR* cszPipeType = TEXT("Control");
        return cszPipeType;
    }
#endif // DEBUG

    USHORT              GetNumTDsNeeded( const STransfer *pTransfer = NULL ) const;

    BOOL                AreTransferParametersValid( const STransfer *pTransfer = NULL ) const;

    HCD_REQUEST_STATUS  ScheduleTransfer( void );

    HCD_REQUEST_STATUS  AddTransfer( STransfer *pTransfer );

    BOOL ProcessShortTransfer( PUHCD_TD pTD );

    // ****************************************************
    // Private variables for CControlPipe
    // ****************************************************
};

class CInterruptPipe : public CQueuedPipe
{
public:
    // ****************************************************
    // Public Functions for CInterruptPipe
    // ****************************************************
    CInterruptPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                    IN const BOOL fIsLowSpeed,
                    IN CUhcd *const pCUhcd);
    ~CInterruptPipe();

    virtual PIPE_TYPE GetType () { return TYPE_INTERRUPT; };

    HCD_REQUEST_STATUS    OpenPipe( void );

    // ****************************************************
    // Public variables for CInterruptPipe
    // ****************************************************

private:
    // ****************************************************
    // Private Functions for CInterruptPipe
    // ****************************************************

#ifdef DEBUG
    const TCHAR*  GetPipeType( void ) const
    {
        static const TCHAR* cszPipeType = TEXT("Interrupt");
        return cszPipeType;
    }
#endif // DEBUG

    void                UpdateInterruptQHTreeLoad( IN const UCHAR branch,
                                                   IN const int   deltaLoad );

    USHORT              GetNumTDsNeeded( const STransfer *pTransfer = NULL ) const;

    BOOL                AreTransferParametersValid( const STransfer *pTransfer = NULL ) const;

    HCD_REQUEST_STATUS  ScheduleTransfer( void );

    HCD_REQUEST_STATUS  AddTransfer( STransfer *pTransfer );

    // ****************************************************
    // Private variables for CInterruptPipe
    // ****************************************************
};

class CIsochronousPipe : public CPipe
{
public:
    // ****************************************************
    // Public Functions for CIsochronousPipe
    // ****************************************************
    CIsochronousPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                      IN const BOOL fIsLowSpeed,
                      IN CUhcd *const pCUhcd);
    ~CIsochronousPipe();

    virtual PIPE_TYPE GetType () { return TYPE_ISOCHRONOUS; };

    HCD_REQUEST_STATUS  OpenPipe( void );

    HCD_REQUEST_STATUS  ClosePipe( void );

    HCD_REQUEST_STATUS  AbortTransfer( 
                                IN const LPTRANSFER_NOTIFY_ROUTINE lpCancelAddress,
                                IN const LPVOID lpvNotifyParameter,
                                IN LPCVOID lpvCancelId );

    // ****************************************************
    // Public variables for CIsochronousPipe
    // ****************************************************

private:
    // ****************************************************
    // Private Functions for CIsochronousPipe
    // ****************************************************
#ifdef DEBUG
    const TCHAR*  GetPipeType( void ) const
    {
        static const TCHAR* cszPipeType = TEXT("Isochronous");
        return cszPipeType;
    }
#endif // DEBUG

    USHORT              GetNumTDsNeeded( const STransfer *pTransfer = NULL ) const;

    BOOL                AreTransferParametersValid( const STransfer *pTransfer = NULL ) const;

    DWORD               GetMemoryAllocationFlags( void ) const;

    HCD_REQUEST_STATUS  ScheduleTransfer( void );

    HCD_REQUEST_STATUS  AddTransfer( STransfer *pTransfer );

    BOOL                CheckForDoneTransfers( void );

//    static void         InsertIsochTDIntoFrameList( IN_OUT PUHCD_TD pTD,
//                                                    IN const DWORD dwFrameIndex );
//
//    static void         RemoveIsochTDFromFrameList( IN_OUT PUHCD_TD pTD,
//                                                    IN const DWORD dwFrameIndex );

    // ****************************************************
    // Private variables for CIsochronousPipe
    // ****************************************************
    PUHCD_TD            m_pWakeupTD;    // TD used to schedule transfers far into future
    BOOL                m_fUsingWakeupTD; // indicates if m_pWakeupTD is being used
#define ISOCH_TD_WAKEUP_INTERVAL    DWORD(50) // wakeup TD will be scheduled 50 frames before
                                              // the first real TD should be executed
    CUhcd * const m_pCUhcd;
};

// This is the maximum number of frames of isoch data that can be queued
// successfully. These frames can be in one or more distinct transfers.
#define ISOCH_STREAMING_MAX  1000

#endif // __CPIPE_HPP__

