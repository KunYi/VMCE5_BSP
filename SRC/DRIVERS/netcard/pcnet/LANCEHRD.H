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

    lancehrd.h

Abstract:

    This file contains the hardware-related definitions for
    the LANCE driver.

Environment:

    This driver is expected to work in DOS, OS2 and NT at the equivalent
    of kernel mode.

    Architecturally, there is an assumption in this driver that we are
    on a little endian machine.

Revision History:

$Log:   V:/pvcs/archives/network/pcnet/homelan/wince/mini4/src/LANCEHRD.H_y  $
 * 
 *    Rev 1.0   Apr 07 1999 18:11:56   jagannak
 * Initial check-in
 * 
 *    Rev 1.9   Mar 31 1999 12:08:18   cabansag
 *  
 * 
 *    Rev 1.58   30 Sep 1997 15:22:38   steiger
 * Added LanceMutex function to I/O port macros for debug use.
 * "#define MTX" must be un-commented at the top of the file to enable.
 * 
 * 
 *    Rev 1.57   20 Aug 1997 14:23:50   steiger
 * Further refinements to multi-send and multi-receive.
 * 
 * 
 *    Rev 1.56   12 Aug 1997 17:51:04   steiger
 * Multi-RX implemented. Copy/Compare testing has been done.
 * 
 *    Rev 1.55   31 Jul 1997 14:52:22   steiger
 * Interim check-in.
 * -Multi-send implemented.
 * -Interrupt routine cleaned up somewhat.
 * -Send routine cleaned up.
 *  
 * 
 *    Rev 1.54   10 Jul 1997 17:01:42   steiger
 * Cleaned up LED and Full Duplex init.
 * Added real-time line speed detection for support of OID_GEN_LINK_SPEED.
 * 
 * 
 *    Rev 1.53   07 Jul 1997 17:53:52   steiger
 * Interim check in -- Added multi-modal link status check.
 * Redundant mode is affected by this change.
 * Redundant mode is untested.
 * 
 *    Rev 1.52   30 Jun 1997 18:27:30   steiger
 * Completed support for external PHY media selection.
 * 
 *    Rev 1.51   07 May 1997 18:43:18   steiger
 * Added Redundant mode to driver.

--*/
#if DBG
//#define MTX	1	/* Turns on mutex checking in port read/write macros */
#endif

#define MAX_PACKET_FRAGMENT   4

/* mode register */
#define LANCE_PROMISCIOUS_MODE    0x8000        /* promiscuous receiver mode */
#define LANCE_NORMAL_MODE    0x0000        /* Normal operation mode */

/* Common xxRing->hadr flags */

#define OWN     0x80
#define DERR    0x40  /* descriptor ERR, not CSR0 ERR */
#define STP     0x02
#define ENP     0x01

/* Unique TxRing->hadr flags */

#define LANCE_TRANSMIT_MORE_COLLISION     0x10  /* frame sent after 2 or more collisions */
#define LANCE_TRANSMIT_ONE_COLLISION     0x08  /* frame sent after 1 collision */
#define LANCE_TRANSMIT_DEF_ERROR         0x04  /* frame sent after channel busy dropped */

/* Unique RxRing->hadr flags */

#define LANCE_RECEIVE_FRAME_ERROR    0x20 /* framing error */
#define LANCE_RECEIVE_OVERFLOW_ERROR 0x10 /* SILO overflow error */
#define LANCE_RECEIVE_CRC_ERROR         0x08 /* Frame checksum error */
#define LANCE_RECEIVE_BUFFER_ERROR   0x04 /* buffer overrun error */

/* TxRing->status flags */

#define LANCE_TRANSMIT_BUF_ERROR			0x8000
#define LANCE_TRANSMIT_UFLO_ERROR		0x4000
#define LANCE_TRANSMIT_EXDEF_ERROR		0x2000
#define LANCE_TRANSMIT_LCOL_ERROR     	0x1000
#define LANCE_TRANSMIT_LCAR_ERROR     	0x0800
#define LANCE_TRANSMIT_RTRY_ERROR     	0x0400

/* BCR Registers */

#define	LANCE_LED0_STAT		4		/* LED 0 Status Reg */
#define	LANCE_LED_ON		0x8000	/* State of LED */
#define	LANCE_LINKSE		0x0040	/* Value to write to enable read of link status */
#define	LANCE_LINK_FDE		0x0100	/* Value to write to enable read of FD link status */
#define	LANCE_LINKSE_MASK	0x7FFF	/* Mask to restore contents of LED_STAT reg */

#define LANCE_BCR7    7    /* BCR7 Register */
#define LANCE_BCR18   18   /* BCR18 Register */

/* Full Duplex Control REGister (BCR9) */

#define LANCE_BCR9			9
#define LANCE_FDC_REG		9
#define LANCE_FDC_FDEN		0x0001	/* Full Duplex ENable bit

/* define bcr7 bits: */
#define LANCE_BCR7_MPSE       0x0200
#define LANCE_BCR7_PSE        0x0080
#define LANCE_BCR18_BREADE    0x0040
#define LANCE_BCR18_BWRITE    0x0020

/* define bcr2 */
#define LANCE_BCR2				2
#define LANCE_BCR2_ASEL			0x0002
#define LANCE_BCR2_LEDPE		0x1000	/* PCnet-FAST LED programming */

/* Define BCR49 : New register in Everest */
#define LANCE_BCR49				49
#define PHY_SEL_LOCK				0x0080
#define PHY_SEL_MASK				0xFFFC
#define PHY_SEL_DEFAULT_MASK	0x0300
#define PHY_SEL_DEFAULT_BIT	8

/* PHY Selection flag */
#define PHY_SEL_SFEX				0x0000
#define PHY_SEL_HOMERUN			0x0001
#define PHY_SEL_EXTERNAL		0x0002
#define FIND_ACTIVE_PHY			0x0003

/* Defs for reading link status from [MII */
/* Used in function LanceReadLink() */
#define	LS0				0x04	/* Link Status Bit */
#define	MII_STAT_REG	0x01	/* Offset of status register */

// for Homelan
#define	MII_IEEE_ID		0x02	/* Offset of IEEE ID register */
#define	MII_IEEE_ID1	0x03	/* Offset of IEEE ID register */
#define	MII_MY_ABILITY	0x04	/* Auto-negotiation advertisement ability register */
#define	MII_LNK_PRTNR	0x05	/* Auto-negotiation link partner ability register */
#define	ABILITY_MASK	0x1FC0	/* Mask to isolate the technology ability field */
#define	MII_ADDR		33		/* AKA BCR33, used to access tranceiver */
#define	MII_MDR			34		/* AKA BCR34, used to read data from tranceiver */
#define 	LANCE_BCR33	 33
#define 	LANCE_BCR34	 34


// Generic IDs of Auto-Negotiation Registers (common for all PHYs).
#define ANR0        0x0000

/* ANR0 (common for all PHYs) */
#define ANR0_PR     0x8000          // PHY Reset: 1 (external PHY).
#define ANR0_LB     0x4000          // Loopback: 1 = Within external PHY.
#define ANR0_SS     0x2000          // Speed Selection: 1 = 100 Mb/s.
#define ANR0_ANE    0x1000          // Auto-Negotiation Enable: 0 = disabled.
#define ANR0_DM     0x0100          // Duplex Mode: 1 = Full Duplex.
#define ANR0_ISO	  0x0400				// Isolate Phy from MII

//
//	Definition for the Home Lan.
//

/* HomeRun Phy Control Register */
#define	HR_PHY_CTRL_REG	0x10
#define	C_LOW_POWER			0x0800
#define	C_HIGH_POWER		0x0400
#define	C_LOW_SPEED			0x0200
#define	C_HIGH_SPEED		0x0100
#define	HIGH_SPEED			0x0004
#define	HIGH_POWER			0x0002
#define  HIGH_POWER_BIT		1
#define	AID_COMMAND_MASK	0xF0FF

/* HomeRun Phy Status Register */
#define 	HR_PHY_STATUS_REG		0x11
#define	LINK_PULSE_DISABLE	0x1000
#define	RECEIVED_POWER			0x0040
#define	RECEIVED_SPEED			0x0020
#define	RECEIVED_SPEED_BIT 	5

/* HomeRun Analog Test Register 0x1F */
#define 	HR_ANALOG_TEST_REG		0x1F
#define 	FORCE_LINK_VALID			0x0080

/* HomeRun Drive Level Control */
#define 	DEFAULT_LEVEL_CTRL	0x1589

/* AutoConfiguration */
#define DEFAULT_CRC_PER_SEC			5
#define DEFAULT_CONSECUTIVE_CRC		2

#define NUMBER_OF_HOMERUN_PHY_STATE	3

/* HomeLan PHY Address */
#define HOMERUN_PHY_ADDRESS	(0x1D << 5)
#define SFEX_PHY_ADDRESS		(0x1E	<< 5)	//Default Phy for HomeLan, same as PHYADDR in lancehrd.h
#define EXT_PHY_END_POINT		0x1D

#define PHY_ADDRESS_MASK		(0x1F << 5)

/* Address of internal PHY, aligned starting at bit 5 */
#define	PHYADDR			(0x01 << 5)

/* External PHY register and bit definitions */
/* Relevant register is BCR32 */
/* Bit names match those in the Datasheet so you don't need to keep looking */
/* at this file for the bit names !!! ;-> */

#define	LANCE_BCR32		32		/* Register offset */
#define	MIIPD			0x4000	/* MII PHY Detect bit */

/* 0x2000 thru 0x0100 may be defined in HW, but not used in software at this time */

#define	DANAS			0x0080	/* Disable Auto-Negotiation auto setup*/
#define	XPHYRST			0x0040	/* External PHY Reset */
#define	XPHYANE			0x0020	/* External PHY Auto-Negotiation Enable */
#define	XPHYFD			0x0010	/* External PHY Full-Duplex Enable */
#define	XPHYSP			0x0008	/* External PHY Speed Select (1=100Mbps) */
#define	MIIuL			0x0004	/* Micro Linear 6692 PHY Select */
#define	MIIILP			0x0002	/* Internal Loopback Enable (MII) */
#define	FCON			0x0001	/* Fast Config Enable */
#define	FORCED_PHY_MASK	0xFF07
/* CSR Registers */

#define LANCE_CSR0    0    /* CSR0 Register */
#define LANCE_CSR1    1    /* CSR1 Register */
#define LANCE_CSR2    2    /* CSR2 Register */
#define LANCE_CSR3    3    /* CSR3 Register */
#define LANCE_CSR4    4    /* CSR4 Register */
#define LANCE_CSR5    5    /* CSR5 Register */
#define LANCE_CSR15   15   /* CSR5 Register */
#define LANCE_CSR88   88   /* CSR88 Register */
#define LANCE_CSR89   89   /* CSR89 Register */
#define LANCE_CSR15   15   /* CSR15 Register */
#define LANCE_CSR58   58   /* CSR58 Register */
#define LANCE_CSR82   82   /* CSR82 Register */
#define LANCE_CSR125  125  /* CSR125 Register */
#define LANCE_CSR116	 116  /* CSR116 Register for PM */

/* csr0 bits: */
#define LANCE_CSR0_INIT    0x0001
#define LANCE_CSR0_START   0x0002
#define LANCE_CSR0_STOP    0x0004
#define LANCE_CSR0_TDMD    0x0008
#define LANCE_CSR0_TXON    0x0010
#define LANCE_CSR0_RXON    0x0020
#define LANCE_CSR0_IENA    0x0040
#define LANCE_CSR0_INTR    0x0080
#define LANCE_CSR0_IDON    0x0100
#define LANCE_CSR0_TINT    0x0200  /* got a tx int */
#define LANCE_CSR0_RINT    0x0400  /* got a recieve int */
#define LANCE_CSR0_MERR    0x0800
#define LANCE_CSR0_MISS    0x1000
#define LANCE_CSR0_CERR    0x2000
#define LANCE_CSR0_BABL    0x4000
#define LANCE_CSR0_ERR     0x8000  /* MERR | MISS | CERR | BABL */
#define LANCE_CSR0_RUNNING 0x0033  /* Value to check that the chip is running  */
#define    LANCE_CSR0_CLEAR   0xFF00
/* define csr3 bits: */
#define LANCE_CSR3_BCON    0x0001
#define LANCE_CSR3_ACON    0x0002
#define LANCE_CSR3_BSWP    0x0004
#define LANCE_CSR3_TINTM   0x0200
#define LANCE_CSR3_TINTM_IDONM 0x0300
#define LANCE_CSR3_IDONM   0x0100
#define LANCE_CSR3_BABLM   0x4000
#define LANCE_CSR3_MERRM   0x0800
#define LANCE_CSR3_DXSUFLO 0x0040

/* define csr4 bits: */
#define LANCE_CSR4_DMAPLUS 0x4000
#define LANCE_CSR4_AUTOPADTRANSMIT 0x0800
#define LANCE_CSR4_DPOLL   0x1000
#define LANCE_CSR4_TIMER   0x2000

/* define csr5 bits: */
#define LANCE_CSR5_SPND		0x0001
#define LANCE_CSR5_MPMODE	0x0002
#define LANCE_CSR5_MPEN		0x0004
#define LANCE_CSR5_MPPLBA	0x0020

/* define csr15 bits: */
#define LANCE_CSR15_DRX   0x0001
#define LANCE_CSR15_DTX   0x0002

/* define csr125 IPG value: */
#define LANCE_CSR125_IPG  0x5c00
#define LANCE_CSR125_HLIPG  0x6400
#define IPGMASK			  0x00FF	

//
// Controller I/O address
//
#define LANCE_RDP_PORT   0x10
#define LANCE_RAP_PORT   0x12
#define LANCE_RESET_PORT 0x14
#define LANCE_IDP_PORT   0x16

//
//   Default configuration settings
//
#define LANCE_DEFAULT_BASE_ADDRESS        0x300       /* default IO base                  */
#define LANCE_DEFAULT_INTERRUPT_VECTOR    3           /* default IRQ vector               */
#define LANCE_DEFAULT_INTERRUPT_LEVEL     3           /* default IRQ level                */
#define LANCE_DEFAULT_DMA_CHANNEL         5           /* default DMA channel              */

//
// The MAC header size for ethernet
//
#define MAC_HEADER_SIZE (UINT)14

//
// The maximum number of bytes that we will pass to an NDIS
// indication (since we receive packets contiguously, there is
// no reason to limit this).
//
#define LANCE_INDICATE_MAXIMUM     1514

//
// The maximum number of bytes we will pass to a loopback
// indication (unless it all is in one buffer). This number
// includes data only, not the header.
//

#define LANCE_LOOPBACK_MAXIMUM 208

//
// Maximum number of multicast entries in the table
//
#define LANCE_MAX_MULTICAST      32

/* Default transmit settings	*/

/* Default # of buffers	*/
#define TRANSMIT_BUFFERS         32


#ifdef NDIS40_PLUS
 /* Threshold at which transmit buffer resources are freed */
 #define	TX_INT_WATERMARK		TRANSMIT_BUFFERS / 2
 #define	RX_INT_WATERMARK		RECEIVE_BUFFERS / 2
#endif

/* Default size of transmit buffer	*/

#define TRANSMIT_BUFFER_SIZE    1536

//
// Default receive settings
//

//
// Default # of buffers
//
#define RECEIVE_BUFFERS          32

//
// Default size of receive buffer
//
#define RECEIVE_BUFFER_SIZE     1536

//
// Minimum packet size for Ethernet.
//
#define LANCE_MIN_PACKET_SIZE    60

#if 0
//
// Device IDs.
//
#define    PCNET_ISA_ID           0x00003003
#define    PCNET_ISA_PLUS_ID      0x02260003
#define    PCNET_ISA_PLUS_PLUS_ID 0x02261003
#define    HiLance_ID             0x02430003

//
// Revision IDs.
//
#define    PCNET_ISA_B2_REV_ID    0x4
#define    PCNET_PCI2_A4_REV_ID   0x12
#define    PCNET_PCI2_B2_REV_ID   0x16
#define    PCNET_PCI3_REV_ID      0x20
#endif //0

#define    PCNET_HOME_A1_REV_ID   0x50
#define    PCNET_HOME_B0_REV_ID   0x51
#define    PCNET_HOME_B1_REV_ID   0x52

//
// Lance Initialization Block
//

typedef struct _LANCE_INIT_BLOCK {

   //
   // Mode register
   //
   USHORT Mode;

   //
   // Physical Network Address
   //
   UCHAR PhysicalNetworkAddress[ETH_LENGTH_OF_ADDRESS];

   //
   // Logical Address Fileter
   //
   UCHAR LogicalAddressFilter[8];

   //
   // Receive descriptor ring pointer - low 16 bits
   //
   USHORT ReceiveDescriptorRingPhysicalLow;

   //
   // Receive descriptor ring pointer - high 8 bits (23:16) + RLEN
   //
   UCHAR ReceiveDescriptorRingPhysicalHighL;

   UCHAR RLen;

   //
   // Transmit descriptor ring pointer - low 16 bits
   //
   USHORT TransmitDescriptorRingPhysicalLow;

   //
   // Transmit descriptor ring pointer - high 8 bits (23:16) + TLEN
   //
   UCHAR TransmitDescriptorRingPhysicalHighL;

   UCHAR TLen;

   } LANCE_INIT_BLOCK, *PLANCE_INIT_BLOCK;

typedef struct _LANCE_INIT_BLOCK_HI {

      //
      // Mode register
      //
      USHORT Mode;

      //
      // Number of Receive Descriptor Ring Entries.
      //
      UCHAR RLen;

      //
      // Number of Transmit Descriptor Ring Entries.
      //
      UCHAR TLen;

      //
      // Physical Network Address
      //
      UCHAR PhysicalNetworkAddress[ETH_LENGTH_OF_ADDRESS];

      //
      // Reserved word.
      //
      USHORT Reserved;

      //
      // Logical Address Fileter
      //
      UCHAR LogicalAddressFilter[8];

      //
      // Receive descriptor ring pointer - low 16 bits
      //
      USHORT ReceiveDescriptorRingPhysicalLow;

      //
      // Receive descriptor ring pointer - high 8 bits 
      //
      UCHAR ReceiveDescriptorRingPhysicalHighL;

      //
      // Receive descriptor ring pointer - high 8 bits 
      //
      UCHAR ReceiveDescriptorRingPhysicalHighH;
	   
      //
      // Transmit descriptor ring pointer - low 16 bits
      //
      USHORT TransmitDescriptorRingPhysicalLow;

      //
      // Transmit descriptor ring pointer - high 8 bits 
      //
      UCHAR TransmitDescriptorRingPhysicalHighL;

      //
      // Transmit descriptor ring pointer - high 8 bits 
      //
      UCHAR TransmitDescriptorRingPhysicalHighH;


      } LANCE_INIT_BLOCK_HI, *PLANCE_INIT_BLOCK_HI;

//
// Exponent of buffer length.
//
#define BUFFER_LENGTH_EXPONENT    0x20
#define BUFFER_LENGTH_EXPONENT_H  0x10

//
// Descriptor size
//
#define LANCE_DESCRIPTOR_SIZE 8
#define LANCE_DESCRIPTOR_HI_SIZE 16


typedef struct _LANCE_TRANSMIT_DESCRIPTOR_HI {

     //
     // A Physical pointer to a buffer - low 16 bits.
     //
     USHORT LanceBufferPhysicalLow;

     //
     // A Physical pointer to a buffer - low 8 bits
     //
     UCHAR LanceBufferPhysicalHighL;

     //
     // A Physical pointer to a buffer - high 8 bits
     //
     UCHAR LanceBufferPhysicalHighH;


     //
     // Holds the length of data placed into the buffer.  This
     // should be less than the actual buffer length.
     // Bits 15:12 should always be ones.
     //
     SHORT ByteCount;

     //
     // Reserved Byte.
     //
     UCHAR LanceTMDReserved1;

     //
     // Transmit Message Flags.
     //
     UCHAR LanceTMDFlags;

     //
     // Transmit Retry Count. (15:4) Reserved.
     //
     USHORT LanceTMDTrc;

     //
     // Time Domain Reflectrometry (TDR)(25:16) + Error bits (26:31)
     //
     USHORT TransmitError;

     //
     // Reserved word 2
     //
     USHORT Reserved2;

     //
     // Reserved word 3
     //
     USHORT Reserved3;

   } LANCE_TRANSMIT_DESCRIPTOR_HI,*PLANCE_TRANSMIT_DESCRIPTOR_HI;

typedef struct _LANCE_TRANSMIT_DESCRIPTOR {

    //
    // A Physical pointer to a buffer - low 16 bits.
    //
    USHORT LanceBufferPhysicalLow;

    //
    // A Physical pointer to a buffer - high 8 bits + flags
    //
    UCHAR LanceBufferPhysicalHighL;

    //
    // Transmit Message Flags.
    //
    UCHAR LanceTMDFlags;

    //
    // Holds the length of data placed into the buffer.  This
    // should be less than the actual buffer length.
    // Bits 15:12 should always be ones.
    //
    SHORT ByteCount;

    //
    // Time Domain Reflectrometry (TDR)(9:0) + Error bits (15:10)
    //
    USHORT TransmitError;

    } LANCE_TRANSMIT_DESCRIPTOR,*PLANCE_TRANSMIT_DESCRIPTOR;

//
// Lance receive desriptor structure
//

typedef struct _LANCE_RECEIVE_DESCRIPTOR_HI {

     //
     // A Physical pointer to a buffer - low 16 bits.
     //
     USHORT LanceBufferPhysicalLow;
	 
     //
     // A Physical pointer to a buffer - high 8 bits + flags
     //
     UCHAR LanceBufferPhysicalHighL;

     //
     // A Physical pointer to a buffer - high 8 bits + flags
     //
     UCHAR LanceBufferPhysicalHighH;

     //
     // Size of the buffer pointed by this discriptor
     // Bits 15:12 should always be ones.
     //
     SHORT BufferSize;

     //
     // Reserved Byte.
     //
     UCHAR LanceRMDReserved1;

     //
     // Received Message Flags.
     //
     UCHAR LanceRMDFlags;

     //
     // Holds the length of data placed into the buffer.  This
     // should be less that the actual buffers lengths
     //
     USHORT ByteCount;

     //
     // Runt Packet Count.
     //
     UCHAR LanceRMDRpCnt;

     //
     // Receive Collision Count.
     //
     UCHAR LanceRMDRcCnt;

     //
     // Reserved word.
     //
     USHORT LanceRMDReserved2;

     //
     // Reserved word.
     //
     USHORT LanceRMDReserved3;

   } LANCE_RECEIVE_DESCRIPTOR_HI,*PLANCE_RECEIVE_DESCRIPTOR_HI;

typedef struct _LANCE_RECEIVE_DESCRIPTOR {

    //
    // A Physical pointer to a buffer - low 16 bits.
    //
    USHORT LanceBufferPhysicalLow;

    //
    // A Physical pointer to a buffer - high 8 bits + flags
    //
    UCHAR LanceBufferPhysicalHighL;

    //
    // Received Message Flags.
    //
    UCHAR LanceRMDFlags;

    //
    // Size of the buffer pointed by this discriptor
    // Bits 15:12 should always be ones.
    //
    SHORT BufferSize;

    //
    // Holds the length of data placed into the buffer.  This
    // should be less that the actual buffers lengths
    //
    USHORT ByteCount;

    } LANCE_RECEIVE_DESCRIPTOR,*PLANCE_RECEIVE_DESCRIPTOR;

/*****************************************************************************/

#if MTX	/* Macros using LanceMutex. NEVER use in free builds */
//
// Handy macros to read Lance I/O ports.
//
#define LANCE_READ_CSR_BEFORE_REGISTRATION(_IOBASE, _Port, _Value, _Handle) \
{\
	LanceMutex (LOCK);	\
    NdisImmediateWritePortUshort(_Handle, _IOBASE + LANCE_RAP_PORT, (USHORT)(_Port)); \
    NdisImmediateReadPortUshort(_Handle, _IOBASE + LANCE_RDP_PORT, (PUSHORT)(_Value)); \
	LanceMutex (UNLOCK);	\
}

#define LANCE_READ_CSR_BEFORE_REGISTRATION(_IOBASE, _Port, _Value, _Handle) \
{\
	LanceMutex (LOCK);	\
    NdisImmediateWritePortUshort(_Handle, _IOBASE + LANCE_RAP_PORT, (USHORT)(_Port)); \
    NdisImmediateReadPortUshort(_Handle, _IOBASE + LANCE_RDP_PORT, (PUSHORT)(_Value)); \
	LanceMutex (UNLOCK);	\
}
#define LANCE_READ_CSR(_IOBASE, _Port, _Value) \
{\
	LanceMutex (LOCK);	\
    NdisRawWritePortUshort(_IOBASE + LANCE_RAP_PORT, (USHORT)(_Port)); \
    NdisRawReadPortUshort(_IOBASE + LANCE_RDP_PORT, (PUSHORT)(_Value)); \
	LanceMutex (UNLOCK);	\
}
#define LANCE_WRITE_CSR(_IOBASE, _Port, _Value) \
{\
	LanceMutex (LOCK);	\
    NdisRawWritePortUshort(_IOBASE + LANCE_RAP_PORT, (USHORT)(_Port)); \
    NdisRawWritePortUshort(_IOBASE + LANCE_RDP_PORT, (USHORT)(_Value)); \
	LanceMutex (UNLOCK);	\
}

#define LANCE_READ_BCR_BEFORE_REGISTRATION(_IOBASE, _Port, _Value, _Handle) \
{\
	LanceMutex (LOCK);	\
    NdisImmediateWritePortUshort(_Handle, _IOBASE + LANCE_RAP_PORT, (USHORT)(_Port)); \
    NdisImmediateReadPortUshort(_Handle, _IOBASE + LANCE_IDP_PORT, (PUSHORT)(_Value)); \
	LanceMutex (UNLOCK);	\
}

#define LANCE_WRITE_BCR_BEFORE_REGISTRATION(_IOBASE, _Port, _Value, _Handle) \
{\
	LanceMutex (LOCK);	\
    NdisImmediateWritePortUshort(_Handle, _IOBASE + LANCE_RAP_PORT, (USHORT)(_Port)); \
    NdisImmediateWritePortUshort(_Handle, _IOBASE + LANCE_IDP_PORT, (USHORT)(_Value)); \
	LanceMutex (UNLOCK);	\
}

#define LANCE_READ_BCR(_IOBASE, _Port, _Value) \
{\
	LanceMutex (LOCK);	\
    NdisRawWritePortUshort(_IOBASE + LANCE_RAP_PORT, (USHORT)(_Port)); \
    NdisRawReadPortUshort(_IOBASE + LANCE_IDP_PORT, (PUSHORT)(_Value)); \
	LanceMutex (UNLOCK);	\
}

#define LANCE_WRITE_BCR(_IOBASE, _Port, _Value) \
{\
	LanceMutex (LOCK);	\
    NdisRawWritePortUshort(_IOBASE + LANCE_RAP_PORT, (USHORT)(_Port)); \
    NdisRawWritePortUshort(_IOBASE + LANCE_IDP_PORT, (USHORT)(_Value)); \
	LanceMutex (UNLOCK);	\
}

/****************************************************************************/

#else /* #if MTX */

//
// Handy macros to read Lance I/O ports.
//
#define LANCE_READ_CSR_BEFORE_REGISTRATION(_IOBASE, _Port, _Value, _Handle) \
{\
    NdisImmediateWritePortUshort(_Handle, _IOBASE + LANCE_RAP_PORT, (USHORT)(_Port)); \
    NdisImmediateReadPortUshort(_Handle, _IOBASE + LANCE_RDP_PORT, (PUSHORT)(_Value)); \
}

#define LANCE_READ_CSR_BEFORE_REGISTRATION(_IOBASE, _Port, _Value, _Handle) \
{\
    NdisImmediateWritePortUshort(_Handle, _IOBASE + LANCE_RAP_PORT, (USHORT)(_Port)); \
    NdisImmediateReadPortUshort(_Handle, _IOBASE + LANCE_RDP_PORT, (PUSHORT)(_Value)); \
}
#define LANCE_READ_CSR(_IOBASE, _Port, _Value) \
{\
    NdisRawWritePortUshort(_IOBASE + LANCE_RAP_PORT, (USHORT)(_Port)); \
    NdisRawReadPortUshort(_IOBASE + LANCE_RDP_PORT, (PUSHORT)(_Value)); \
}
#define LANCE_WRITE_CSR(_IOBASE, _Port, _Value) \
{\
    NdisRawWritePortUshort(_IOBASE + LANCE_RAP_PORT, (USHORT)(_Port)); \
    NdisRawWritePortUshort(_IOBASE + LANCE_RDP_PORT, (USHORT)(_Value)); \
}

#define LANCE_READ_BCR_BEFORE_REGISTRATION(_IOBASE, _Port, _Value, _Handle) \
{\
    NdisImmediateWritePortUshort(_Handle, _IOBASE + LANCE_RAP_PORT, (USHORT)(_Port)); \
    NdisImmediateReadPortUshort(_Handle, _IOBASE + LANCE_IDP_PORT, (PUSHORT)(_Value)); \
}

#define LANCE_WRITE_BCR_BEFORE_REGISTRATION(_IOBASE, _Port, _Value, _Handle) \
{\
    NdisImmediateWritePortUshort(_Handle, _IOBASE + LANCE_RAP_PORT, (USHORT)(_Port)); \
    NdisImmediateWritePortUshort(_Handle, _IOBASE + LANCE_IDP_PORT, (USHORT)(_Value)); \
}

#define LANCE_READ_BCR(_IOBASE, _Port, _Value) \
{\
    NdisRawWritePortUshort(_IOBASE + LANCE_RAP_PORT, (USHORT)(_Port)); \
    NdisRawReadPortUshort(_IOBASE + LANCE_IDP_PORT, (PUSHORT)(_Value)); \
}

#define LANCE_WRITE_BCR(_IOBASE, _Port, _Value) \
{\
    NdisRawWritePortUshort(_IOBASE + LANCE_RAP_PORT, (USHORT)(_Port)); \
    NdisRawWritePortUshort(_IOBASE + LANCE_IDP_PORT, (USHORT)(_Value)); \
}

//
// Handy macros to read/write to MII ports.
//

#define LANCE_READ_MII(_IOBASE, _AddrValue, _DataValue) \
{\
	LANCE_WRITE_BCR (_IOBASE, MII_ADDR, _AddrValue); \
	LANCE_READ_BCR (_IOBASE, MII_MDR, _DataValue); \
}

#define LANCE_WRITE_MII(_IOBASE, _AddrValue, _DataValue) \
{\
	LANCE_WRITE_BCR (_IOBASE, MII_ADDR, _AddrValue); \
	LANCE_WRITE_BCR (_IOBASE, MII_MDR, _DataValue); \
}


#endif /* #if MTX */
/****************************************************************************/

//
// Macros to get MSB and LSB of an address.
//

#define LANCE_GET_LOW_PART_ADDRESS(Adr) ((USHORT)((Adr) & 0xffff))
#define LANCE_GET_HIGH_PART_ADDRESS(Adr) ((UCHAR)(((Adr) & 0xff0000) >> 16))
#define LANCE_GET_HIGH_PART_ADDRESS_H(Adr) ((UCHAR)(((Adr) & 0xff000000) >> 24))
#define LANCE_GET_HIGH_PART_PCI_ADDRESS(Adr) ((USHORT)(((Adr) & 0xffff0000) >> 16))

