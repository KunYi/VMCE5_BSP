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
//     CHW.hpp
// 
// Abstract: Provides interface to UHCI host controller
// 
// Notes: 
//

#ifndef __CHW_HPP__
#define __CHW_HPP__

#include <globals.hpp>
//#include "cuhcd.hpp"
#include <cphysmem.hpp>
#include <Hcd.hpp>

class CHW;

// this class is an encapsulation of UHCI hardware registers.
class CHW : public CHcd
{
public:
    // ****************************************************
    // public Functions
    // ****************************************************

    // 
    // Hardware Init/Deinit routines
    //
    CHW( IN const REGISTER portBase,
                              IN const DWORD dwSysIntr,
                              IN CPhysMem * const pCPhysMem,
                              //IN CUhcd * const pHcd,
                              IN LPVOID pvUhcdPddObject );
    ~CHW(); 
    virtual BOOL    Initialize();
    virtual void    DeInitialize( void );
    virtual void    SignalCheckForDoneTransfers( void ) { DebugBreak();};
    
    void   EnterOperationalState(void);

    void   StopHostController(void);

    //
    // Functions to Query frame values
    //
    BOOL GetFrameNumber( OUT LPDWORD lpdwFrameNumber );

    BOOL GetFrameLength( OUT LPUSHORT lpuFrameLength );
    
    BOOL SetFrameLength( IN HANDLE hEvent,
                                IN USHORT uFrameLength );
    
    BOOL StopAdjustingFrame( void );

    BOOL WaitOneFrame( void );

    //
    // Root Hub Queries
    //
    BOOL DidPortStatusChange( IN const UCHAR port );

    BOOL GetPortStatus( IN const UCHAR port,
                               OUT USB_HUB_AND_PORT_STATUS& rStatus );

    BOOL RootHubFeature( IN const UCHAR port,
                                IN const UCHAR setOrClearFeature,
                                IN const USHORT feature );

    BOOL ResetAndEnablePort( IN const UCHAR port );

    void DisablePort( IN const UCHAR port );

    //
    // Miscellaneous bits
    //
    PULONG GetFrameListAddr( ) { return m_pFrameList; };
    // PowerCallback
    VOID PowerMgmtCallback( IN BOOL fOff );
    BOOL    SuspendHC();

private:
    // ****************************************************
    // private Functions
    // ****************************************************
    
    static DWORD CALLBACK CeResumeThreadStub( IN PVOID context );
    DWORD CeResumeThread();
    static DWORD CALLBACK UsbInterruptThreadStub( IN PVOID context );
    DWORD UsbInterruptThread();

    static DWORD CALLBACK UsbAdjustFrameLengthThreadStub( IN PVOID context );
    DWORD UsbAdjustFrameLengthThread();

    void   UpdateFrameCounter( void );
    VOID    SuspendHostController();
    VOID    ResumeHostController();

#ifdef DEBUG
    // Query Host Controller for registers, and prints contents
    void DumpUSBCMD(void);
    void DumpUSBSTS(void);
    void DumpUSBINTR(void);
    void DumpFRNUM(void);
    void DumpFLBASEADD(void);
    void DumpSOFMOD(void);
    void DumpAllRegisters(void);
    void DumpPORTSC( IN const USHORT port );
#endif

    //
    // UHCI USB I/O registers (See UHCI spec, section 2)
    //
    
    // UHCI Spec - Section 2.1.1
    // USB Command Register (USBCMD)
    // 16 bits (R/W)
    // bits 15:8 - reserved
    #define UHCD_USBCMD_MASK                        USHORT(0xFF)
    #define UHCD_USBCMD_MAX_RECLAMATION_SIZE_64     USHORT(1 << 7)
    #define UHCD_USBCMD_CONFIGURE_FLAG              USHORT(1 << 6)
    #define UHCD_USBCMD_SOFTWARE_DEBUG              USHORT(1 << 5)
    #define UHCD_USBCMD_FORCE_GLOBAL_RESUME         USHORT(1 << 4)
    #define UHCD_USBCMD_ENTER_GLOBAL_SUSPEND_MODE   USHORT(1 << 3)
    #define UHCD_USBCMD_GLOBAL_RESET                USHORT(1 << 2)
    #define UHCD_USBCMD_HOST_CONTROLLER_RESET       USHORT(1 << 1)
    #define UHCD_USBCMD_RUN_STOP                    USHORT(1 << 0)

    inline USHORT Read_USBCMD( void )
    {
        DEBUGCHK( m_portBase != 0 );
        return READ_PORT_USHORT( ((PUSHORT) m_portBase) );
    }
    inline void Write_USBCMD( IN const USHORT data )
    {
        DEBUGCHK( m_portBase != 0 );
    #ifdef DEBUG // added this after accidentally writing to USBCMD instead of USBSTS
        if ( data & (UHCD_USBCMD_SOFTWARE_DEBUG | UHCD_USBCMD_FORCE_GLOBAL_RESUME | UHCD_USBCMD_ENTER_GLOBAL_SUSPEND_MODE | UHCD_USBCMD_GLOBAL_RESET | UHCD_USBCMD_HOST_CONTROLLER_RESET ) ) { 
            DEBUGMSG( ZONE_WARNING, (TEXT("!!!Warning!!! Setting resume/suspend/reset bits of USBCMD\n")));
        }
    #endif // DEBUG
        WRITE_PORT_USHORT( ((PUSHORT)m_portBase), data );
    }
    
    // UHCI Spec - Section 2.1.2
    // USB Status Register (USBSTS)
    // 16 bits (R/WC)
    // bits 15:6 - reserved
    #define UHCD_USBSTS_MASK                          USHORT(0x3F)
    #define UHCD_USBSTS_HCHALTED                      USHORT(1 << 5)
    #define UHCD_USBSTS_HOST_CONTROLLER_PROCESS_ERROR USHORT(1 << 4)
    #define UHCD_USBSTS_HOST_SYSTEM_ERROR             USHORT(1 << 3)
    #define UHCD_USBSTS_RESUME_DETECT                 USHORT(1 << 2)
    #define UHCD_USBSTS_USB_ERROR_INTERRUPT           USHORT(1 << 1)
    #define UHCD_USBSTS_USB_INTERRUPT                 USHORT(1 << 0)
    #define UHCD_USBSTS_ERROR_MASK                    USHORT(UHCD_USBSTS_HCHALTED | UHCD_USBSTS_HOST_CONTROLLER_PROCESS_ERROR | UHCD_USBSTS_HOST_SYSTEM_ERROR | UHCD_USBSTS_USB_ERROR_INTERRUPT)
    
    inline USHORT Read_USBSTS( void )
    {
        DEBUGCHK( m_portBase != 0 );
        return READ_PORT_USHORT( ((PUSHORT)(m_portBase + 0x2)) );
    }
    inline void Write_USBSTS( IN const USHORT data )
    {
        DEBUGCHK( m_portBase != 0 );
        WRITE_PORT_USHORT( ((PUSHORT)(m_portBase + 0x2)), data );
    }
    inline void Clear_USBSTS( void )
    {
        // write to USBSTS will clear contents
        Write_USBSTS( UHCD_USBSTS_MASK );
    }

    // UHCI Spec - Section 2.1.3
    // USB Interrupt Enable Register (USBINTR)
    // 16 bits (R/W)
    // bits 15:4 - reserved
    #define UHCD_USBINTR_MASK                   USHORT(0xF)
    #define UHCD_USBINTR_SHORT_PACKET_INTERRUPT USHORT(1 << 3)
    #define UHCD_USBINTR_INTERRUPT_ON_COMPLETE  USHORT(1 << 2)
    #define UHCD_USBINTR_RESUME_INTERRUPT       USHORT(1 << 1)
    #define UHCD_USBINTR_TIMEOUT_CRC_INTERRUPT  USHORT(1 << 0)
    
    inline USHORT Read_USBINTR( void )
    {
        DEBUGCHK( m_portBase != 0 );
        return READ_PORT_USHORT( ((PUSHORT)(m_portBase + 0x4)) );
    }
    inline void Write_USBINTR( IN const USHORT data )
    {
        DEBUGCHK( m_portBase != 0 );
        WRITE_PORT_USHORT( ((PUSHORT)(m_portBase + 0x4)), data );
    }

    // UHCI Spec - Section 2.1.4
    // USB Frame Number Register (FRNUM)
    // 16 bits (R/W)
    // bits 15:11 - reserved
    #define UHCD_FRNUM_MASK                     USHORT(0x7FF)
    #define UHCD_FRNUM_INDEX_MASK               USHORT(0x3FF)
    #define UHCD_FRNUM_COUNTER_MSB              USHORT(0x400)

    inline USHORT Read_FRNUM( void )
    {
        DEBUGCHK( m_portBase != 0 );
        return READ_PORT_USHORT( ((PUSHORT)(m_portBase + 0x6)) );
    }
    inline void Write_FRNUM( IN const USHORT data )
    {
        DEBUGCHK( m_portBase != 0 );
        WRITE_PORT_USHORT( ((PUSHORT)(m_portBase + 0x6)), data );
    }
    
    // UHCI Spec - Section 2.1.5
    // USB Frame List Base Address Register (FLBASEADD)
    // 32 bits (R/W)
    // bits 31:12 Base Address
    #define UHCD_FLBASEADD_MASK                 DWORD(0xFFFFF000)
    // bits 11:0 reserved
    
    inline ULONG Read_FLBASEADD( void )
    {
        DEBUGCHK( m_portBase != 0 );
        return READ_PORT_ULONG( ((PULONG)(m_portBase + 0x8)) );
    }
    inline void Write_FLBASEADD( IN const ULONG data )
    {
        DEBUGCHK( m_portBase != 0 );
        // low bits must always be written as zeros
        DEBUGCHK( (data & UHCD_FLBASEADD_MASK) == data );
        WRITE_PORT_ULONG( ((PULONG)(m_portBase + 0x8)), data );
    }

    // UHCI Spec - Section 2.1.6
    // Start of Frame (SOF) Modify Register (SOFMOD)
    // 8 bits (R/W)
    #define UHCD_SOFMOD_MASK                    UCHAR(0x7F)
    // bit 7 - reserved
    // bits 6:0 - SOF Timing Value - add to 11936 to get the Frame Length
    #define UHCD_SOFMOD_MINIMUM_LENGTH          USHORT(11936)
    #define UHCD_SOFMOD_MAXIMUM_LENGTH          USHORT(12063)
    
    inline UCHAR Read_SOFMOD( void )
    {
        DEBUGCHK( m_portBase != 0 );
        return READ_PORT_UCHAR( ((PUCHAR)(m_portBase + 0xC)) );
    }
    inline void Write_SOFMOD( IN const UCHAR data )
    {
        DEBUGCHK( m_portBase != 0 );
        // should only be writing 0 to 127 to this register
        DEBUGCHK( (data & UHCD_SOFMOD_MASK) == data );
        WRITE_PORT_UCHAR( ((PUCHAR)(m_portBase + 0xC)), data );
    }
    
    // UHCI Spec - Section 2.1.7
    // Port Status and Control Register (PORTSC)
    // 16 bits (R/W)
    #define UHCD_PORTSC_MASK                    USHORT(0x137F)
    // bits 15:13 reserved
    #define UHCD_PORTSC_SUSPEND                 USHORT(1 << 12)
    // bits 11:10 reserved
    #define UHCD_PORTSC_PORT_RESET              USHORT(1 << 9)
    #define UHCD_PORTSC_LOW_SPEED_DEVICE        USHORT(1 << 8)
    // bit 7 - reserved (always read as 1)
    #define UHCD_PORTSC_RESUME_DETECT           USHORT(1 << 6)
    // bits 5:4 - line status
    #define UHCD_PORTSC_LINE_STATUS             USHORT((1 << 5)|(1 << 4))
    #define UHCD_PORTSC_PORT_ENABLE_CHANGE      USHORT(1 << 3)
    #define UHCD_PORTSC_PORT_ENABLED            USHORT(1 << 2)
    #define UHCD_PORTSC_CONNECT_STATUS_CHANGE   USHORT(1 << 1)
    #define UHCD_PORTSC_CONNECT_STATUS          USHORT(1 << 0)
    // #define UHCD_PORTSC_WRITE_MASK           USHORT(UHCD_PORTSC_SUSPEND | UHCD_PORTSC_PORT_RESET | UHCD_PORTSC_RESUME_DETECT | UHCD_PORTSC_PORT_ENABLED)

    inline USHORT Read_PORTSC( IN const UINT port )
    {
        DEBUGCHK( m_portBase != 0 );
        // check that we're trying to read a valid port
        DEBUGCHK( HCD_NUM_ROOT_HUB_PORTS == 2 );
        DEBUGCHK( port == 1 || port == 2 );
        // port #1 is at 0x10, port #2 is at 0x12
        return READ_PORT_USHORT( ((PUSHORT)(m_portBase + 0xE + 2 * port)) );
    }
    inline void Write_PORTSC( IN const UINT port, IN const USHORT data )
    {
        DEBUGCHK( m_portBase != 0 );
        // check that we're trying to read a valid port
        DEBUGCHK( port == 1 || port == 2 );
        // port #1 is at 0x10, port #2 is at 0x12.
        WRITE_PORT_USHORT( ((PUSHORT)(m_portBase + 0xE + 2 * port)), data );   
    }

    // ****************************************************
    // Private Variables
    // ****************************************************

    REGISTER m_portBase;

    // internal frame counter variables
    CRITICAL_SECTION m_csFrameCounter;
    DWORD    m_frameCounterHighPart;
    USHORT   m_frameCounterLowPart;

    // interrupt thread variables
    DWORD    m_dwSysIntr;
    HANDLE   m_hUsbInterruptEvent;
    HANDLE   m_hUsbInterruptThread;
    BOOL     m_fUsbInterruptThreadClosing;

    // frame length adjustment variables
    // note - use LONG because we need to use InterlockedTestExchange
    LONG     m_fFrameLengthIsBeingAdjusted;
    LONG     m_fStopAdjustingFrameLength;
    HANDLE   m_hAdjustDoneCallbackEvent;
    USHORT   m_uNewFrameLength;
    PULONG   m_pFrameList;

    DWORD   m_dwCapability;
    BOOL    m_bDoResume;
public:
    DWORD   SetCapability(DWORD dwCap); 
    DWORD   GetCapability() { return m_dwCapability; };
private:
    // initialization parameters for the IST to support CE resume
    // (resume from fully unpowered controller).
    //CUhcd    *m_pHcd;
    CPhysMem *m_pMem;
    LPVOID    m_pPddContext;
private:
    BOOL g_fPowerUpFlag ;
    BOOL g_fPowerResuming ;
public:
    BOOL GetPowerUpFlag() { return g_fPowerUpFlag; };
    BOOL SetPowerUpFlag(BOOL bFlag) { return (g_fPowerUpFlag=bFlag); };
    BOOL GetPowerResumingFlag() { return g_fPowerResuming ; };
    BOOL SetPowerResumingFlag(BOOL bFlag) { return (g_fPowerResuming=bFlag) ; };
};
#endif // __CHW_HPP__

