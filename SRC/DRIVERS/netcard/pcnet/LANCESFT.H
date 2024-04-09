/*++

Copyright (c) 1996 ADVANCED MICRO DEVICES, INC. All Rights Reserved.
This software is unpblished and contains the trade secrets and 
confidential proprietary information of AMD. Unless otherwise provided
in the Software Agreement associated herewith, it is licensed in confidence
"AS IS" and is not to be reproduced in whole or part by any means except
for backup. Use, duplication, or disclosure by the Government is subject
to the restrictions in paragraph (b) (3) (B) of the Rights in Technical
Data and Computer Software clause in DFAR 52.227-7013 (a) (Oct 1988).
Software owned by Advanced Micro Devices, Inc., 901 Thompson Place,
Sunnyvale, CA 94088.

Module Name:

	lancesft.h

Abstract:

	The main header for a LANCE NDIS driver.

Environment:

	This driver is expected to work in DOS, OS2 and NT at the equivalent
	of kernel mode.

	Architecturally, there is an assumption in this driver that we are
	on a little endian machine.

Created:

	09/30/92

Revision History:

$Log:   V:/pvcs/archives/network/pcnet/homelan/wince/mini4/src/LANCESFT.H_y  $
 * 
 *    Rev 1.0   Apr 07 1999 18:12:06   jagannak
 * Initial check-in
 * 
 *    Rev 1.9   Mar 31 1999 12:08:18   cabansag
 *  
 * 
 *    Rev 1.60   30 Sep 1997 15:23:10   steiger
 * Added LanceMutex function prototype and associated constants for debug use.
 * 
 * 
 * 
 *    Rev 1.59   20 Aug 1997 14:23:52   steiger
 * Further refinements to multi-send and multi-receive.
 * 
 * 
 *    Rev 1.58   12 Aug 1997 17:51:06   steiger
 * Multi-RX implemented. Copy/Compare testing has been done.
 * 
 *    Rev 1.57   31 Jul 1997 14:52:24   steiger
 * Interim check-in.
 * -Multi-send implemented.
 * -Interrupt routine cleaned up somewhat.
 * -Send routine cleaned up.
 *  
 * 
 *      Rev 1.56        10 Jul 1997 17:01:46    steiger
 * Cleaned up LED and Full Duplex init.
 * Added real-time line speed detection for support of OID_GEN_LINK_SPEED.
 * 
 * 
 *      Rev 1.55        30 Jun 1997 18:27:32    steiger
 * Completed support for external PHY media selection.
 * 
 *      Rev 1.52        07 May 1997 19:05:06    steiger
 * Removed conditional statements for redundancy, replaced with "NDIS40_PLUS"

--*/
//#include "temp.h"

#ifndef _LANCESFT_
#define _LANCESFT_

#define LANCE_DRIVER_MAJOR_VERSION 1
#define LANCE_DRIVER_MINOR_VERSION 0

#ifdef NDIS50_MINIPORT
#define LANCE_DRIVER_NAME       "PCNTN5HL"
#elif defined(NDIS40_MINIPORT)
#define LANCE_DRIVER_NAME       "PCNTN4HL"
#else
#define LANCE_DRIVER_NAME       "PCNTN3HL"
#endif

/* We use STATIC to define procedures that will be static in the */
/* final build but which we now make extern to allow them to be */
/* debugged. */


#if DEVL
 #define STATIC
#else
 #define STATIC static
#endif

// for DMI
#ifdef DMI
/* ADAPTER STRUCTURE VERSION for AMDDMI.DLL (MAJ.MIN)*/
#define ADAPTER_STRUCTURE_DMI_VERSION_MAJ       0x1             /* One Nibble */
#define ADAPTER_STRUCTURE_DMI_VERSION_MIN       0x0             /* One Nibble */

#define ADAPTER_STRUCTURE_DMI_VERSION   ((ADAPTER_STRUCTURE_DMI_VERSION_MAJ << 4) | ADAPTER_STRUCTURE_DMI_VERSION_MIN)
#endif

/* This variable is used to control debug output. */

#if DBG
 extern INT LanceDbg;
 extern INT LanceHLDbg;
 extern INT LancePMDbg;
 extern INT LanceSendDbg;
 extern INT LanceExtPhyDbg;
 extern INT LanceEventDbg;
 extern INT LanceRxDbg;
 extern INT LanceTxQueueDbg;
 extern INT LanceREQDbg;
 extern INT LanceRegChkDbg;
 extern INT LanceFilterDbg;
 extern INT LanceInitDbg;
 extern INT LanceTempDbg;
 extern INT LanceBreak;
#endif

#if DBG
 #define LANCELOG 1
#else
 #define LANCELOG 0
#endif

#if LANCELOG
 #define LOG_SIZE 256
 #define TIMER                                  '.'
 #define IN_ISR                                 'i'
 #define OUT_ISR                                'I'
 #define IN_DPC                                 'd'
 #define OUT_DPC                                'D'
 #define RECEIVE                                'R'
 #define TRANSMIT                               'x'
 #define TRANSMIT_COMPLETE              'X'
 #define PEND                                   'p'
 #define UNPEND                                 'P'
 #define INDICATE                               'r'
 #define IN_SEND                                's'
 #define OUT_SEND                               'S'
 #define START                                  'G'
 #define RESET_STEP_1                   '1'
 #define RESET_STEP_2                   '2'
 #define RESET_SAVE_PACKET              'b'
 #define RESET_RECOVER_PACKET   'B'
 #define RESET_COMPLETE_PACKET  'c'
 #define RESET_STEP_3                   '3'
 #define REMOVE                                 'V'
 #define CLOSE                                  'C'
 #define UNLOAD                                 'U'
 #define MISSED                                 'M'
 #define ERR                                    'E'
 #define BABL                                   '?'
 #define HEART                                  'H'
 extern UCHAR                                   Log[LOG_SIZE];
 extern UCHAR                                   LogPlace;
 #define LOG(c)                                 { Log[LogPlace++] = (c); Log[LogPlace+3] ='\0'; }
#else
 #define LOG(c)
#endif

/*
	Maximum Phy supported by HomeLan
*/
#define MAX_EXT_PHYS                            3
#define MAX_NEIGHBOR_STORAGE    10                      // cannot be greater than 16

typedef struct _HomeLan_Auto_Config {
	ULONG   stateHistory;
	ULONG   MaxAllowedCRCs;
	ULONG   numberOfCRCs;
	ULONG   numberOfExcludeSources;
	UCHAR   excludeSources[MAX_NEIGHBOR_STORAGE][ETH_LENGTH_OF_ADDRESS];
	UCHAR   CRCSources[MAX_NEIGHBOR_STORAGE][ETH_LENGTH_OF_ADDRESS];
	ULONG   CRCStates;
	ULONG   RcvStateValid;
//      NDIS_MINIPORT_TIMER HomeLanTimer;
} HomeLan_Auto_Config;  

#define INIT_STATE_MASK 0xC0000000
#define STATE_BIT_MASK  0x00000003
#define STATE_BITS      2
#define INIT_STATE_BITS 30

#define STATE_LPHS      0x0001
#define STATE_HPHS      0x0002
#define STATE_HPLS      0x0003
#define STATE_LPLS      0x0004

#define HOMELAN_TIMEOUT 1000            // 1 second

/*
 * Structure Definition for Pattern Match
*/

#ifdef NDIS50_PLUS

#define MAX_ALLOWED_PATTERN     8               //Fast+ supports upto 8 pattern match
#define MAX_PMR_ADDRESS                 64              //Size of Pattern Match RAM is 64 * 5 bytes
#define BASE_PM_START_ADDRESS   2               //First 2 address will be used as a pointer
#define MAX_BLOCKS                              20              //Pattern Match Block + Free Block
												//Free Block = PM Block + 1;


typedef struct _PM_DATABYTES {
	UCHAR   DataByte0;
	UCHAR DataByte1;
	UCHAR DataByte2;
	UCHAR DataByte3;                                        //Data byte 3, 2, 1, 0
}PM_DATABYTES, *PPM_DATABYTES;

//typedef ULONG PM_DATABYTES;

typedef struct _PMR {
	PM_DATABYTES    pmDataBytes[MAX_PMR_ADDRESS];
	USHORT  PatternControl[MAX_PMR_ADDRESS];
} PMR, *PPMR;

typedef struct _PM_BLOCK {
	LIST_ENTRY      linkage;
	ULONG   startAddress;                                   // start address of the Block
	ULONG   blockSize;                                              // size of the Block, unit of 5 bytes
} PM_BLOCK, *PPM_BLOCK;

typedef struct _PMR_MANAGER {
	ULONG   totalPMRUsed;
	ULONG   patterns;                                               // Number of patterns in PMR
	LIST_ENTRY      patternList;                    // Linked list for pattern
	ULONG   freeBlocks;                                             // Number of free blocks in PRM
	LIST_ENTRY      freeList;                               // Linked list for free Blocks  
	PM_BLOCK        pmBlock[MAX_BLOCKS];
	LIST_ENTRY      pmBlockPool;                    // hold all the unused blocks
	PMR pmRAM;
} PMR_MANAGER, *PPMR_MANAGER;

#endif //NDIS50_PLUS

#if 0
/* Constants for FindRedundantAdapter () */
#define UPWARDS                                         00
#define DOWNWARDS                                       01
#define NUM_SLOTS_TO_SEARCH                     16
#define  LOADTIMEOUT            1000
#endif //0

/* Default size of protocol reserved area of NDIS packet descriptors    */
#define PROT_RESERVED_AREA_SIZE         0

/* Used when registering ourselves with NDIS.   */
#ifdef NDIS40_MINIPORT
 #define LANCE_NDIS_MAJOR_VERSION       0x04
 #define LANCE_NDIS_MINOR_VERSION       0x00
 #define MAX_SEND_PACKETS 4
 #define MAX_SEND_PACKETS_HL 1
#elif NDIS50_MINIPORT
 #define LANCE_NDIS_MAJOR_VERSION       0x05
 #define LANCE_NDIS_MINOR_VERSION       0x00
 #define MAX_SEND_PACKETS 4
 #define MAX_SEND_PACKETS_HL 1
#else
 #define LANCE_NDIS_MAJOR_VERSION       0x03
 #define LANCE_NDIS_MINOR_VERSION       0x00
#endif


/* Used for parsing OIDs        */
#define OID_TYPE_MASK                                           0xffff0000
#define OID_TYPE_GENERAL_OPERATIONAL            0x00010000
#define OID_TYPE_GENERAL_STATISTICS                     0x00020000
#define OID_TYPE_802_3_OPERATIONAL                      0x01010000
#define OID_TYPE_802_3_STATISTICS                       0x01020000
#define OID_TYPE_POWER_MANAGEMENT               0xFD010000

#define OID_REQUIRED_MASK                                       0x0000ff00
#define OID_REQUIRED_MANDATORY                          0x00000100
#define OID_REQUIRED_OPTIONAL                           0x00000200

#define OID_INDEX_MASK                                          0x000000ff


/* Indexes in the GeneralMandatory array.       */
#define GM_TRANSMIT_GOOD                                0x00
#define GM_RECEIVE_GOOD                                 0x01
#define GM_TRANSMIT_BAD                                 0x02
#define GM_RECEIVE_BAD                                  0x03
#define GM_RECEIVE_NO_BUFFER                    0x04
#define GM_ARRAY_SIZE                                   0x05


/* Indexes in the GeneralOptional array. There are      */
/* two sections, the ones up to COUNT_ARRAY_SIZE        */
/* have entries for number (4 bytes) and number of      */
/* bytes (8 bytes), the rest are a normal array.        */


#define GO_DIRECTED_TRANSMITS                   0x00
#define GO_MULTICAST_TRANSMITS                  0x01
#define GO_BROADCAST_TRANSMITS                  0x02
#define GO_DIRECTED_RECEIVES                    0x03
#define GO_MULTICAST_RECEIVES                   0x04
#define GO_BROADCAST_RECEIVES                   0x05
#define GO_COUNT_ARRAY_SIZE                             0x06

#define GO_ARRAY_START                                  0x0C
#define GO_RECEIVE_CRC                                  0x0C
#define GO_TRANSMIT_QUEUE_LENGTH                0x0D
#define GO_ARRAY_SIZE                                   0x0E


/* Indexes in the MediaMandatory array. */
#define MM_RECEIVE_ERROR_ALIGNMENT              0x00
#define MM_TRANSMIT_ONE_COLLISION               0x01
#define MM_TRANSMIT_MORE_COLLISIONS             0x02
#define MM_ARRAY_SIZE                                   0x03


/* Indexes in the MediaOptional array.  */
#define MO_TRANSMIT_DEFERRED                    0x00
#define MO_TRANSMIT_MAX_COLLISIONS              0x01
#define MO_RECEIVE_OVERRUN                              0x02
#define MO_TRANSMIT_UNDERRUN                    0x03
#define MO_TRANSMIT_HEARTBEAT_FAILURE   0x04
#define MO_TRANSMIT_TIMES_CRS_LOST              0x05
#define MO_TRANSMIT_LATE_COLLISIONS             0x06
#define MO_ARRAY_SIZE                                   0x07

// for DMI
#ifdef DMI
/* Indexes in the DMI Specific array.   */
typedef enum DmiSpecific
{
	DMI_EX_DEFER,
	DMI_TX_BYTES,
	DMI_RX_BYTES,
	DMI_CSR0_BABL,
	DMI_CSR0_MERR,
	DMI_CSR0_ERR,
	DMI_RXBUFF_ERR,
	DMI_TXBUFF_ERR,
	DMI_RESET_CNT,
	DMI_SPECIFIC_SIZE       // Always last.
};
#endif

/*      Scan parameters for the board search across the buses   */
#define LED_DEFAULT             (ULONG)~0       /* Default for Led0,1 & 2                               */
#if 0
#define FDUP_DEFAULT    (USHORT)~0      /* Default for Full Duplex Mode                 */
#define BUSTIMER_DEFAULT 60                     /* Dflt for Csr82 (Bus Activity timer)  */
#define FDUP_OFF                0x0000          /* Full Duplex Mode turned off                  */
#define FDUP_AUI                0x0003          /* AUI Mode turned on                                   */
#define FDUP_10BASE_T   0x0001          /* 10Base-T Mode turned on                              */
#define PART_NO_MASK    0x0003          /* PCNetISA ID MASK                                             */
#endif //0
#define LINESPEED_DEFAULT 10            /* PCnet default line speed                     */

/* Device bus type      */
#if 0
#define PCI_DEV                 0x01            /* PCI bus flag                                 */
#define PLUG_PLAY_DEV   0x02            /* PnP ISA bus flag                             */
#define LOCAL_DEV               0x03            /* Local bus flag                               */
#define ISA_DEV                 0x04            /* ISA bus flag                                 */
#endif //0

/* MapRegisters */
#define DEFAULT_MAP_REG_COUNT   2

/* Device type on ISA bus       */
#if 0
#define LANCE                           0x00    /* LANCE present FLAG                           */
#define PCNET_ISA                       0x01    /* PCNetISA flag                                        */
#define PCNET_ISA_PLUS          0x02    /* PCNetISA + present flag                      */
#define PCNET_ISA_PLUS_PLUS     0x03    /* PCNetISA ++ present flag                     */
#define PCNET_PCI2_A4           0x04    /* PCNetPCI 2 rev A4 present flag       */
#define PCNET_PCI2_B2           0x05    /* PCNetPCI 2 rev B2 present flag       */
#define PCNET_PCI3                      0x06    /* PCNetPCI 3 present flag                      */
#define PCNET_PCI1                      0x07    /* PCNetPCI 3 present flag                      */
#endif //0

#define PCNET_HOME_A1                   0x0001
#define PCNET_HOME_B0                   0x0002
#define PCNET_HOME_B1                   0x0003

/* Chip ID in CSR 9 :   */
/*      LANSHARK - PCnet-ISA+ (Am79C961)        */
/*      HILANCE - PCnet-VL      (Am79C965)      */
#if 0
#define LANSHARK                        0x01            /* LANSHARK dev ID      (offset 09)     */      
#define HILANCE                         0x10            /* HILANCE dev ID (offset 09)   */      
#define PCNET_PCI_ID            0x20001022      /* vendor/device id -- Presidio */      
#define ISA_IRQ_DEF                     3                       /* default IRQ for ISA                  */
#define ISA_DMA_DEF                     5                       /* default DMA for ISA                  */
#endif
#define PCNET_HL_PCI_ID         0x20011022      /* vendor/device id -- Presidio */      
//#define PCNET_HL_PCI_ID               0x20001022      /* vendor/device id -- Presidio */      

#define LANCE_INIT_OK                   0       /* LanceHardwareDetails return codes */
#define LANCE_INIT_WARNING_8    8
#define LANCE_INIT_ERROR_9              9
#define LANCE_INIT_ERROR_10             10
#define LANCE_INIT_ERROR_11             11
#define LANCE_INIT_ERROR_12             12
#define LANCE_INIT_ERROR_13             13
#define LANCE_INIT_ERROR_14             14
#define LANCE_INIT_ERROR_15             15
#define LANCE_INIT_ERROR_16             16
#define LANCE_INIT_ERROR_17             17
#define LANCE_INIT_ERROR_18             18
#define LANCE_INIT_ERROR_19             19
#define LANCE_INIT_ERROR_20             20
#define LANCE_INIT_ERROR_21             21
#define LANCE_INIT_ERROR_22             22
#define LANCE_INIT_ERROR_23             23
#define LANCE_INIT_ERROR_24             24
#if 0
#define ANCHOR_ID                               0x0701110e              /* Anchorage id */
#define HILANCE_PORT                    0x8800                  /* Hilance port in Anchorage */
#endif //0
#define NO_DMA                                  0

/* PCI register offset  */
#define PCI_ID_REG                              0x00
#define PCI_COMMAND_REG                 0x04
#define PCI_REV_ID_REG                  0x08
#define PCI_BASE_IO_REG                 0x10
#define PCI_INTR_REG                    0x3C

#ifdef NDIS50_PLUS
#define PCI_STATUS_REG                  0x06
#define PCI_CAP_ID_REG                  0x40    // PCI Capability ID Register
#define PCI_PMC_REG                             0x42  // Power Management Capabilities
#define PCI_PMCSR_REG                   0x44    // Power Management Control/Status

#define NEW_CAP                         0x0010  // New Capabilities for PCI_STATUS_REG
#define PME_EN                                  0x0100

#endif //NDIS50_PLUS

/*
Time window (unspecified units of time) allowed waiting
for chip to start and/or stop. This value was determined
empirically; should allow for several seconds to pass
before giving up. Used in LanceStartChip() and
LanceStopChip().
*/

#define START_STOP_TIMEOUT              8192    /* Arbitrarily chosen */

#if DBG /* Defined for Mutex Function */
UCHAR LanceMutex (USHORT Flag);
#define LOCK    1
#define UNLOCK  0
#endif

#ifndef DMIDLL

/* Macros used for memory allocation and deallocation.  */

#ifdef NDIS50_MINIPORT
#define LANCE_ALLOC_MEMORY(_Status, _Address, _Length)                                  \
{                                                                                                                                               \
	NDIS_PHYSICAL_ADDRESS Temp = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);       \
    *(_Status) = NdisAllocateMemoryWithTag(                                                                        \
					(PVOID)(_Address),									\
					(_Length), 	 										\
                                        'AMDN');                                                                                                      \
}
#else
#define LANCE_ALLOC_MEMORY(_Status, _Address, _Length)                                  \
{                                                                                                                                               \
	NDIS_PHYSICAL_ADDRESS Temp = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);       \
	*(_Status) = NdisAllocateMemory(                                                                        \
					(PVOID)(_Address),                                                                      \
					(_Length),                                                                                      \
					0,                                                                                                      \
					Temp                                                                                            \
					);                                                                                                      \
}
#endif

#define LANCE_FREE_MEMORY(Address, Length)                                                              \
	NdisFreeMemory((PVOID)(Address),(Length), 0)

#define LANCE_ALLOC_CONTIGUOUS_MEMORY(_Status, _Address, _Length)               \
{                                                                                                                                               \
	NDIS_PHYSICAL_ADDRESS Temp = NDIS_PHYSICAL_ADDRESS_CONST(-1, 0);        \
	*(_Status) = NdisAllocateMemory((PVOID)(_Address),                                      \
									(_Length),                                                      \
									NDIS_MEMORY_CONTIGUOUS,                         \
									Temp);                                                          \
}

#define LANCE_FREE_CONTIGUOUS_MEMORY(Address, Length)                                   \
	NdisFreeMemory((PVOID)(Address), (Length), NDIS_MEMORY_CONTIGUOUS)

#define LANCE_ALLOC_NONCACHED_MEMORY(_Status, _Address, _Length)                \
{                                                                                                                                               \
	NDIS_PHYSICAL_ADDRESS Temp = NDIS_PHYSICAL_ADDRESS_CONST(-1, 0);        \
	*(_Status) = NdisAllocateMemory((PVOID)(_Address),                                      \
									(_Length),                                                      \
									(NDIS_MEMORY_CONTIGUOUS |                       \
									NDIS_MEMORY_NONCACHED),                         \
									Temp);                                                          \
}

#define LANCE_FREE_NONCACHED_MEMORY(Address, Length)                                    \
{                                                                                                                                               \
	NdisFreeMemory((PVOID)(Address),                                                                        \
				   (Length),                                                                                    \
				   (NDIS_MEMORY_CONTIGUOUS |                                                    \
				   NDIS_MEMORY_NONCACHED))                                                              \
}


/* Macros to move and zero memory.      */
#define LANCE_MOVE_MEMORY(Destination,Source,Length)                                    \
	NdisMoveMemory(Destination,Source,Length)

#define LANCE_ZERO_MEMORY(Destination,Length)                                                   \
	NdisZeroMemory(Destination,Length)

#ifndef NDIS_NT


/* This useful macro is not defined outside of NT.      */
/* Calculate the address of the base of the structure given its type,   */
/* and an address of a field within the structure.      */

/* jk start 
#define CONTAINING_RECORD(address, type, field)                                                 \
	((type *)((PCHAR)(address) - (PCHAR)(&((type *)0)->field)))
	jk end */

#endif // #ifndef NDIS_NT 

typedef unsigned __int64        COUNTER64;

/* Used to record the 8-byte counters.  */
typedef struct _LANCE_LARGE_INTEGER {
	ULONG LowPart;
	ULONG HighPart;
} LANCE_LARGE_INTEGER, *PLANCE_LARGE_INTEGER;


/* This initializes an 8-byte counter.  */
#define LanceZeroLargeInteger(LargeInteger)             \
{                                                                                               \
	LargeInteger.LowPart = 0L;                                      \
	LargeInteger.HighPart = 0L;                                     \
}


/* This adds a longword to an 8-byte counter.   */
#define LanceAddUlongToLargeInteger(LargeInteger, Ulong)        \
{                                                                                                                       \
	PLANCE_LARGE_INTEGER TmpLarge = (LargeInteger);                 \
	TmpLarge->LowPart += (ULONG)Ulong;                                              \
	if (TmpLarge->LowPart < (ULONG)Ulong)                                   \
	{                                                                                                               \
		++TmpLarge->HighPart;                                                           \
	}                                                                                                               \
}


/* Used to pass parameters to interrupt sync. routine   */
typedef struct _COPY_INFO
{
	ULONG SourceBuffer;
	ULONG TargetBuffer;
	ULONG BytesToCopy;
	BOOLEAN CopyFromDramToSram;
} COPY_INFO, *PCOPY_INFO;


/* This flushes a buffer for write.     */
#define LANCE_FLUSH_WRITE_BUFFER(Buffer)                                        \
	NdisFlushBuffer(Buffer, TRUE)

#else /* #ifndef DMIDLL  */

/* Needed typedefs since NDIS.H cannot be included in the AMDDMI.DLL */
/* source. The adapter structure is truncated when DMIDLL is defined. */

typedef PVOID NDIS_HANDLE, *PNDIS_HANDLE;

#endif  /* #ifndef DMIDLL  */

/* This identifies the type of the packet for quick reference   */
/* in the LANCE_PACKET_RESERVED.PacketType field.       */
#define LANCE_DIRECTED                          0x0001
#define LANCE_MULTICAST                         0x0002
#define LANCE_BROADCAST                         0x0004
#define LANCE_LOOPBACK                          0x0008


/* This is the basic structure that defines the state of an     */
/* adapter. There is one of these allocate per adapter that     */
/* the Lance driver supports.   */

/* When used for the AMDDMI.DLL, DMIDLL must be defined, and the */
/* structure is truncated to avoid *nearly* all the NDIS.H dependencies */

typedef struct _LANCE_ADAPTER
{
/**** DO NOT CHANGE THE ORDER OF THE NEXT FIVE STRUCTURE MEMBERS. */
/**** IF YOU NEED TO ADD ANY STRUCTURE MEMBERS, DO SO AFTER */
/**** "OK TO ADD/MODIFY" COMMENT BELOW. DON'T STICK THEM UP HERE OR /*
/**** AMDDMI.DLL *WILL* BREAK ! */

	/* Miniport handle */
	NDIS_HANDLE LanceMiniportHandle;

	/* Wrapper handle */
	NDIS_HANDLE LanceWrapperHandle;

	/* DMA handle */
	NDIS_HANDLE LanceDmaHandle;

#ifdef DMI
	/* Signature for AMDDMI.DLL */
	UCHAR   DmiIdRev[4];
#endif
/**** DO NOT CHANGE THE ORDER OF THE PRECEDING FIVE STRUCTURE MEMBERS */

#ifndef DMIDLL

/* OK TO ADD/MODIFY STRUCTURE PAST THIS POINT. */

	/* Cable disconnected */
	NDIS_MINIPORT_TIMER CableTimer;
	ULONG CableTimerCounter;

	BOOLEAN CableDisconnected;
	BOOLEAN LinkActive;
	
	NDIS_MINIPORT_TIMER DeferredProcessTimer;

#ifdef NDIS40_PLUS

	NDIS_HANDLE LanceRedMiniportHandle;

#if 0
#ifdef _FAILOVER
	NDIS_MINIPORT_TIMER SecondaryMonitorTimer;
#endif //_FAILOVER
#endif //0
//   NDIS_SPIN_LOCK Lock;                        // to access global variable, ActiveAdapter

#endif //NIDS40_MINIPORT

	/* Holds the interrupt object for this adapter. */
	NDIS_MINIPORT_INTERRUPT LanceInterruptObject;

	/* Holds the interrupt object for the redundant adapter. */
//      NDIS_MINIPORT_INTERRUPT LanceRedundantInterruptObject;

	/* Hardware configuration status */
	ULONG LanceHardwareConfigStatus;

	/* Bus type, i.e., ISA, PCI */
	NDIS_INTERFACE_TYPE BusType;

	/* Device Type, i.e., PCNET-ISA or LANCE */
	UCHAR DeviceType;
	UCHAR MIIPhyDetected;

//      UCHAR RedundantDeviceType;
//      UCHAR RedundantMIIPhyDetected;

//      UCHAR DeviceRevisionId;
	UCHAR   FuncFlags;

	/* PCI adapter slot number */
	ULONG LanceSlotNumber; 
//      ULONG LanceRedundantSlot;

	/* PCI adapter bus number */
	ULONG LanceBusNumber; 
//      ULONG LanceRedundantBus;

	/* Base io addresses */
	ULONG PhysicalIoBaseAddress;
	ULONG MappedIoBaseAddress;
//      ULONG PhysRedundantIoBaseAddress;
//      ULONG MappedRedundantIoBaseAddress;

	/* IRQ, DMA resources */
	NDIS_INTERRUPT_MODE InterruptMode;
	UCHAR LanceInterruptVector;
//      UCHAR LanceRedundantInterruptVector;

	UCHAR LanceDmaChannel;
//      UCHAR   Reserved1;

	/* Dma Timer value in Csr 82. */
//      USHORT BusTimer;

	/* Full Duplex Mode Value. */
	USHORT FullDuplex;

	/* Line Speed Value. */
	USHORT LineSpeed;

	/* Board Type found. ISA+, ISA, LOCAL, PCI */
//      UCHAR BoardFound;
//      UCHAR   Reserved2;

	/* Keywords for the adapter. */
//      ULONG BusScan;
//      ULONG tp;
//      ULONG MpMode;
//      ULONG RedundantMode;
	ULONG ExtPhyMode;
	ULONG led0;
	ULONG led1;
	ULONG led2;
	ULONG led3;

	/* Memory allocated */
	ULONG AllocatedNonCachedMemorySize;
	ULONG AllocatedCachedMemorySize;

	/* Physical address of shared memory. */
	NDIS_PHYSICAL_ADDRESS SharedMemoryPa;

	/* Virtual address of shared memory. */
	PVOID SharedMemoryVa;

	/* Physical address of shared memory. */
	NDIS_PHYSICAL_ADDRESS SharedCachedMemoryPa;

	/* Virtual address of shared memory. */
	PVOID SharedCachedMemoryVa;

	/* The pointer to transmit buffers. */
	PCHAR TransmitBufferPointer;

	/* The physical address of the TransmitBufferPointer */
	NDIS_PHYSICAL_ADDRESS TransmitBufferPointerPhysical;

	/* Pointer to the transmit descriptor ring (this is */
	/* allocated to be of size NumberOfTransmitDescriptors). */
	PVOID TransmitDescriptorRing;

	/* The physical address of the TransmitDescriptorRing. */
	NDIS_PHYSICAL_ADDRESS TransmitDescriptorRingPhysical;

	/* Index to the next available transmit descriptor */
	/* in the descriptor ring. */
	UCHAR NextTransmitDescriptorIndex;

	/* Index to the last processed transmit descriptor */
	/* in the descriptor ring. */
	UCHAR TailTransmitDescriptorIndex;
	USHORT Reserved3;

	/* Transmit packet type array */
	UCHAR TransmitPacketType[TRANSMIT_BUFFERS];

	/* Transmit packet length */
	ULONG TransmitPacketLength[TRANSMIT_BUFFERS];

	/* The pointer to receive buffers */
	PCHAR ReceiveBufferPointer;

	/* The physical address of the ReceiveBufferPointer */
	NDIS_PHYSICAL_ADDRESS ReceiveBufferPointerPhysical;

	/* Pointer to the receive descriptor ring */
	/* (this is allocated to be of size NumberOfReceiveDescriptors). */
	PVOID ReceiveDescriptorRing;

	/* The physical address of ReceiveDescriptorRing */
	NDIS_PHYSICAL_ADDRESS ReceiveDescriptorRingPhysical;

	/* Index to the next available receive descriptor */
	/* in the descriptor ring. */
	USHORT NextReceiveDescriptorIndex;
	USHORT Reserved4;

	/* The intialization block for the adapter */
	PVOID InitializationBlock;

	/* The physical address of the Initialization block */
	NDIS_PHYSICAL_ADDRESS InitializationBlockPhysical;

	/* Holds a value of the CSR0 from one or more interrupts. */
	/* This value is only set by the interrupt service routine */
	/* and cleared by the interrupt synchronization routine. */
	USHORT Csr0Value;
	USHORT CsrFiveValue;
	USHORT SavedRapValue;

	/* Operation flags */
	USHORT OpFlags;

	/* These hold adapter statistics. */
	ULONG GeneralMandatory[GM_ARRAY_SIZE];
	LANCE_LARGE_INTEGER GeneralOptionalByteCount[GO_COUNT_ARRAY_SIZE];
	ULONG GeneralOptionalFrameCount[GO_COUNT_ARRAY_SIZE];
	ULONG GeneralOptional[GO_ARRAY_SIZE - GO_ARRAY_START];
	ULONG MediaMandatory[MM_ARRAY_SIZE];
	ULONG MediaOptional[MO_ARRAY_SIZE];
//for DMI
#ifdef DMI
	COUNTER64 DmiSpecific[DMI_SPECIFIC_SIZE];
#endif
	/* The current packet filter. */
	ULONG CurrentPacketFilter;

	/* These hold multicast address information */
	ULONG NumberOfAddresses;

//      UCHAR MulticastAddresses[16][ETH_LENGTH_OF_ADDRESS];
	UCHAR MulticastAddresses[LANCE_MAX_MULTICAST][ETH_LENGTH_OF_ADDRESS];

	/* The burned-in network address from the hardware. */
	UCHAR PermanentNetworkAddress[ETH_LENGTH_OF_ADDRESS];

	/* The current network address from the hardware. */
	UCHAR CurrentNetworkAddress[ETH_LENGTH_OF_ADDRESS];

#ifdef NDIS40_PLUS
	/* Array of NDIS packet pointers for NDIS4 Multi-Rx.    */
	PNDIS_PACKET pNdisPacket[RECEIVE_BUFFERS];
	PNDIS_PACKET PktArray[RECEIVE_BUFFERS];
	
	/* Array of NDIS buffer descriptor pointers for NDIS4 Multi-Rx. */
	PNDIS_BUFFER pNdisBuffer[RECEIVE_BUFFERS];
	PNDIS_BUFFER BufArray[RECEIVE_BUFFERS];

	/* Packet Pool Handle for NDIS4 Multi-Rx */
	NDIS_HANDLE NdisPktPoolHandle;

	/* Buffer Pool Handle for NDIS4 Multi-Rx */
	NDIS_HANDLE NdisBufPoolHandle;
#endif

	/* TxQueue */
	// the pointer to the first packet we have queued in send
	// deserialized miniport support variables
	PNDIS_PACKET        FirstTxQueue;
	PNDIS_PACKET        LastTxQueue;
	UINT                NumPacketsQueued;
	NDIS_SPIN_LOCK            TxQueueLock;

	/* HOME LAN related data fields */
	ULONG           ActivePhyAddress;
	ULONG           DefaultPhyAddress;
	ULONG           NumberOfPhys;
	ULONG           PhyAddress[MAX_EXT_PHYS];
	BOOLEAN isAutoConfigOn;
	BOOLEAN RunBugFix;
	USHORT  LevelCtrl;                      // HomeRun Phy Reg 30
	USHORT  AnalogTest;                     // HomeRun Phy Reg 31
	ULONG           HomeRun0;                       // HomeRun Phy Register programming
										// High order 2 bytes : Register number
										// Low order 2 bytes : Register value
	ULONG           HomeRun1;
	ULONG           IPG;
	ULONG           SyncRate;
	ULONG   HLLinkOff;
	ULONG   HLBackOff;
	ULONG           HomeRunState;
	ULONG           PhySelected;
	HomeLan_Auto_Config     HLAC;

/* Power Management support */
#ifdef NDIS50_PLUS
	/* Power Management RAM manager */
	PMR_MANAGER pmManager;
//      PMR             tempStorage;

	/* Current Power State */
	NDIS_DEVICE_POWER_STATE powerState;
	ULONG WakeUpMode;

#endif //NDIS50_PLUS    

//      COPY_INFO TxCopyInfo;
//      COPY_INFO RxCopyInfo;

	/* Outstanding tx buffer count */
	USHORT  TxBufsUsed;

	/* Software Style used by device */
	USHORT  SwStyle;
	ULONG   CsrNum;
	ULONG   BcrNum;
#endif /* #ifndef DMIDLL */
} LANCE_ADAPTER,*PLANCE_ADAPTER;


// for DMI
#ifdef DMI
/* Constants for LancePortAccess */

/* Lo Nibble */
#define PORT_READ                       0x01
#define PORT_WRITE                      0x02
#define MASK_ACCESS                     0x0F

/* Hi Nibble */
#define CSR_REG                         0x10
#define BCR_REG                         0x20
#define MASK_REGTYPE            0xF0

#define CSR_READ                        (PORT_READ  | CSR_REG)
#define CSR_WRITE                       (PORT_WRITE | CSR_REG)
#define BCR_READ                        (PORT_READ  | BCR_REG)
#define BCR_WRITE                       (PORT_WRITE | BCR_REG)

/* Lance Register Access Type Masks */
/* Bit set indicates that a STOP or SUSPEND */
/* state must be entered before access */

#define LANCE_CSR_MASK0         0xFFFFFF46      /* CSR 0 to 31 */
#define LANCE_CSR_MASK1         0xFFFFFFFF      /* CSR 32 to 63 */
#define LANCE_CSR_MASK2         0xFFFFFFFF      /* CSR 64 to 95 */
#define LANCE_CSR_MASK3         0xFFDFAFFF      /* CSR 96 to 127 */
#define LANCE_BCR_MASK0         0x7E7CFD08      /* BCR 0 to 31 */
#define LANCE_BCR_MASK1         0xFFFFFFF0      /* BCR 32 to 63 */
#define LANCE_BCR_MASK2         0xFFFFFFFF      /* BCR 64 to 95 */
#define LANCE_BCR_MASK3         0xFFFFFFFF      /* BCR 96 to 127 */
#endif //DMI

/* Definitions: Adapter->SwStyle */
#define SW_STYLE_0                      0x0000
#define SW_STYLE_2                      0x0002
#define SW_STYLE_3                      0x0003

/* Operation flag (OpFlags) definitions */
#define RESET_IN_PROGRESS       0x0001
#define IN_INTERRUPT_DPC        0x0002
#define RESET_PROHIBITED        0x0004
#define STOP_SET                        0x0008
#define SUSPENDED                       0x0010
#define INIT_COMPLETED          0x0020
#define TX_ACTION                       0x0040
#define RX_ACTION                       0x0080
#define BYTE_ACCESS                     0x0100
#define RESET_REQUESTED         0x0200
#define TX_RESOURCES            0x0400
#define IN_LINK_TIMER           0x0800
#define RESET_MASK                      ~(STOP_SET | RX_ACTION | TX_ACTION | TX_RESOURCES)
#define PM_MODE                 0x1000
#define LANCE_RESET_CALLED      0x2000
#define LANCE_DEFERRED_SEND_PROCESSING  0x4000

/* Function flag (FuncFlags) definitions */
#define CAP_POWER_MANAGEMENT    0x01
#define CAP_HOME_LAN                            0x02

/* WakeUp Mode */
#define LANCE_WAKE_UP_MAGIC_PACKET              0x00000001
#define LANCE_WAKE_UP_PATTERN_MATCH             0x00000002
#define LANCE_WAKE_UP_LINK_CHANGE               0x00000004


/* Maximum adapters supported by this driver */
#define MAX_ADAPTERS                            4

#ifndef DMIDLL
/* Given a MacBindingHandle this macro returns a pointer to the */
/* LANCE_ADAPTER. */

#define PLANCE_ADAPTER_FROM_BINDING_HANDLE(Handle) \
	(((PLANCE_OPEN)((PVOID)(Handle)))->OwningLance)


/* Given a MacContextHandle return the PLANCE_ADAPTER */
/* it represents. */

#define PLANCE_ADAPTER_FROM_CONTEXT_HANDLE(Handle) \
	((PLANCE_ADAPTER)((PVOID)(Handle)))

/* This macro will return a pointer to the lance reserved portion */
/* of a packet given a pointer to a packet. */
//#define PLANCE_RESERVED_FROM_PACKET(Packet) \
//      ((PLANCE_PACKET_RESERVED)((PVOID)((Packet)->MacReserved)))


/* This structure is used in the MacReserved field of */
/* an NDIS_REQUEST_BLOCK, passed in during multicast */
/* address/packet filter operations. */

typedef struct _LANCE_REQUEST_RESERVED {
	PNDIS_REQUEST Next;
	struct _LANCE_OPEN * OpenBlock;
} _LANCE_REQUEST_RESERVED, * PLANCE_REQUEST_RESERVED;

/* This macro will return a pointer to the lance reserved portion */
/* of a request given a pointer to the request. */
#define PLANCE_RESERVED_FROM_REQUEST(Request) \
	((PLANCE_REQUEST_RESERVED)((PVOID)((Request)->MacReserved)))


/* This macro returns a pointer to a PLANCE_OPEN given a MacBindingHandle. */

#define PLANCE_OPEN_FROM_BINDING_HANDLE(Handle) \
	((PLANCE_OPEN)((PVOID)Handle))


/* This macro returns a NDIS_HANDLE from a PLANCE_OPEN */

#define BINDING_HANDLE_FROM_PLANCE_OPEN(Open) \
	((NDIS_HANDLE)((PVOID)Open))


/* Maximum adapters supported by this driver */
#define MAX_ADAPTERS                            4

#if NDIS40_PLUS
// MJ modified 12 -> 4 and 400 ->1500
 #define        LINK_TIMEOUT                    4 /* number of LINK_FREQ units */
 #define        LINK_FREQ                               1500    /* # of ms for LanceLinkMonitor function */
 #define        CABLE_CHK_TIMEOUT               5000            /* number of LINK_FREQ units */
// #define      LINK_FREQ                               400     /* # of ms for LanceLinkMonitor function */

 /* Defs for Target argument/return codes for function LanceRedundantSwitch ... */
 typedef enum {PRI, RED, QUERY, TOGGLE, UNABLE_TO_SWITCH};

 #define        LINK_UP_COUNT                   2
 #define        LINK_DOWN_COUNT                 2

#endif /* NDIS40_PLUS */

/* procedures which do error logging */
typedef enum _LANCE_PROC_ID{
	registerAdapter,
	openAdapter,
	hardwareDetails,
	handleDeferred,
	processReceiveInterrupts,
	redundantSwitchToPri,
	redundantSwitchToRed
} LANCE_PROC_ID;


/* Error log values */

#define LANCE_ERRMSG_INIT_INTERRUPT                     (ULONG)0x01
#define LANCE_ERRMSG_CREATE_FILTER                      (ULONG)0x02
#define LANCE_ERRMSG_ALLOC_MEMORY                       (ULONG)0x03
#define LANCE_ERRMSG_REGISTER_ADAPTER           (ULONG)0x04
#define LANCE_ERRMSG_ALLOC_DEVICE_NAME          (ULONG)0x05
#define LANCE_ERRMSG_ALLOC_ADAPTER                      (ULONG)0x06
#define LANCE_ERRMSG_INITIAL_INIT                       (ULONG)0x07
#define LANCE_ERRMSG_OPEN_DB                            (ULONG)0x08
#define LANCE_ERRMSG_ALLOC_OPEN                         (ULONG)0x09
#define LANCE_ERRMSG_HARDWARE_ADDRESS           (ULONG)0x0A
#define LANCE_ERRMSG_BOARD_NOT_FOUND            (ULONG)0x0B
#define LANCE_ERRMSG_HARDWARE_ALLOC_BUFFER      (ULONG)0x0C
#define LANCE_ERRMSG_INIT_DMA                           (ULONG)0x0D
#define LANCE_MSG_ADAPTER_SWITCHED              (ULONG)0x0E

/* These are global structures for the MAC. */

extern NDIS_HANDLE LanceMacHandle;
//extern LIST_ENTRY LanceAdapterList;
//extern BOOLEAN LanceUnloading;
//extern NDIS_SPIN_LOCK LanceAdapterListLock;

/* Introduced for cable disconnect indication */
//extern BOOLEAN LinkActive;

//#ifdef _FAILOVER
//extern PLANCE_ADAPTER PrimaryAdapter;
//extern PLANCE_ADAPTER SecondaryAdapter;
//extern ULONG ActiveAdapter;
//#endif

/* The macro to write zero to PCI ID register for bug patch. */
/* The value the data pointer points to should be zero (ulong length) */
#define WRITE_PCI_ID_ZERO(Adapter,DataPtr)                              \
{                                                                                                               \
	NdisWritePciSlotInformation(                                            \
		Adapter->LanceMiniportHandle,                                   \
		Adapter->LanceSlotNumber,                                               \
		PCI_ID_REG,                                                                             \
		DataPtr,                                                                                \
		4);                                                                                             \
}

//
// Public routine definitions
//
VOID
LanceInit(
	IN PLANCE_ADAPTER               Adapter,
	IN BOOLEAN                              FullReset
	);

VOID
LanceMovePacket(
	IN      PLANCE_ADAPTER          Adapter,
		PNDIS_PACKET            Packet,
	IN      UINT                            BytesToCopy,
		PCHAR                           Buffer,
	OUT     PUINT                           BytesCopied,
	IN      BOOLEAN                         CopyFromPacketToBuffer
	);

VOID
LanceStopChip(
	IN      PLANCE_ADAPTER          Adapter
	);

VOID
LanceSetupRegistersAndInit(
	IN      PLANCE_ADAPTER          Adapter,
	IN      BOOLEAN                         FullReset
	);

STATIC
VOID
LanceStartChip(
	IN PLANCE_ADAPTER Adapter
	);

BOOLEAN
LanceSetStartSramAddress(
	IN      PVOID                           AdapterContext
	);

VOID
LanceResetPHY(
	PLANCE_ADAPTER Adapter
	);

VOID
InitHomeRunPort(
	IN PLANCE_ADAPTER Adapter
	);

VOID
LanceHomeLanPortScan(
	IN PLANCE_ADAPTER Adapter
	);

VOID
HomeLanExtPhyConfig(
	IN PLANCE_ADAPTER Adapter
	);

VOID
EverestBugFix(
	PLANCE_ADAPTER Adapter
	);


NDIS_STATUS
LanceSetInformation(
	IN      NDIS_HANDLE                     MiniportAdapterContext,
	IN      NDIS_OID                        Oid,
	IN      PVOID                           InformationBuffer,
	IN      ULONG                           InformationBufferLength,
	OUT     PULONG                          BytesRead,
	OUT     PULONG                          BytesNeeded
	);

NDIS_STATUS
LanceQueryInformation(
	IN      NDIS_HANDLE                     MiniportAdapterContext,
	IN      NDIS_OID                        Oid,
	IN      PVOID                           InformationBuffer,
	IN      ULONG                           InformationBufferLength,
	OUT     PULONG                          BytesWritten,
	OUT     PULONG                          BytesNeeded
	);

VOID
LanceEnableInterrupt(
	IN      NDIS_HANDLE                     MiniportAdapterContext
	);

VOID
LanceDisableInterrupt(
	IN      NDIS_HANDLE                     MiniportAdapterContext
	);

VOID
LanceISR(
	OUT     PBOOLEAN                        InterruptRecognized,
	OUT     PBOOLEAN                        QueueDpc,
	IN      PVOID                           Context
	);

VOID
LanceHandleInterrupt(
	IN      PVOID                           Context
	);

/*
	MJ removed Multi_Send function to check Browser problem.
*/
#ifdef NDIS40_PLUS

VOID
LanceReturnPacket(
	IN      NDIS_HANDLE     MiniportAdapterContext,
	IN PNDIS_PACKET Packet
	);

VOID
LanceSendPackets(
	IN NDIS_HANDLE MiniportAdapterContext,
	IN PPNDIS_PACKET PacketArray,
	IN UINT NumberOfPackets
	);

#else

NDIS_STATUS
LanceSend(
	IN      NDIS_HANDLE                     MiniPortAdapterContext,
	IN      PNDIS_PACKET            Packet,
	IN UINT                                 Flags
	);

#endif

BOOLEAN
LanceAllocateAdapterMemory(
	IN      PLANCE_ADAPTER          Adapter
	);

VOID
LanceDeleteAdapterMemory(
	IN      PLANCE_ADAPTER          Adapter
	);

#ifdef NDIS40_PLUS
/*
VOID
LanceCableTimerFunction(
	IN      PVOID                           SystemSpecific1,
	IN      PLANCE_ADAPTER          Adapter,
	IN      PVOID                           SystemSpecific2,
	IN      PVOID                           SystemSpecific3
	);

STATIC
BOOLEAN
LanceReadLink (
	IN ULONG IoBaseAddress,
	IN UCHAR DevType,
	IN PLANCE_ADAPTER Adapter
	);
*/
#endif 

//jk STATIC
VOID
LanceGetActiveMediaInfo (
	IN PLANCE_ADAPTER Adapter
	);


#endif /* #ifndef DMIDLL */

/* TxQueue */

//jk STATIC
VOID
LanceDeferredSend(
	IN PLANCE_ADAPTER Adapter
	);

//-------------------------------------------------------------------------
// Miniport and MAC specific Defines
//-------------------------------------------------------------------------
typedef struct _LANCE_RESERVED {

    // next packet in the chain of queued packets being allocated,
    // or waiting for the finish of transmission.
    //
    // We always keep the packet on a list so that in case the
    // the adapter is closing down or resetting, all the packets
    // can easily be located and "canceled".
    //
    PNDIS_PACKET Next;
} LANCE_RESERVED,*PLANCE_RESERVED;

#define PLANCE_RESERVED_FROM_PACKET(_Packet) \
	((PLANCE_RESERVED)((_Packet)->MiniportReserved))

#define EnqueuePacket(_Head, _Tail, _Packet)                    \
{                                                               \
	if (!_Head) {                                                   \
		_Head = _Packet;                                        \
	} else {                                                        \
		PLANCE_RESERVED_FROM_PACKET(_Tail)->Next = _Packet;     \
	}                                                       \
	PLANCE_RESERVED_FROM_PACKET(_Packet)->Next = NULL;              \
	_Tail = _Packet;                                                \
}

#define DequeuePacket(Head, Tail)                      \
{                                                      \
	PLANCE_RESERVED Reserved =                          \
	PLANCE_RESERVED_FROM_PACKET(Head);                  \
	if (!Reserved->Next) {                             \
		Tail = NULL;                                       \
	}                                                  \
	Head = Reserved->Next;                             \
}

#endif /* _LANCESFT_ */
