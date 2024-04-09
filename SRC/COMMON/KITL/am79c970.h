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
	Define AMD Am79C970A chipset.
    
Functions:


Notes: 
    
    
--*/

#ifndef _AMD_79C970_HARDWARE_
#define _AMD_79C970_HARDWARE_



/////////////////////////////////////////////////////////////////////////////////
//	CSR and BCR values setting made easy...
//
#define	RESET_REGISTER								0x00000014

#define	CSR0_TDMD									0x00000008
#define	CSR0_RINT									0x00000400
#define	CSR0_IENA									0x00000040


#define	CSR3_BABLM									0x00004000
#define	CSR3_MISSM									0x00001000
#define	CSR3_MERRM									0x00000800
#define	CSR3_RINTM									0x00000400
#define	CSR3_TINTM									0x00000200
#define	CSR3_IDONM									0x00000100
#define	CSR3_MASK_ALL_INTS							CSR3_BABLM | CSR3_MISSM | CSR3_MERRM | CSR3_RINTM | CSR3_TINTM | CSR3_IDONM 

#define	CSR4_DMA_PLUS								0x00004000
#define CSR4_APAD_XMIT								0x00000800
#define	CSR4_MFCOM									0x00000100
#define	CSR4_ASTRP_RCV								0x00000400
#define	CSR4_RCVCCOM                                0x00000010
#define	CSR4_TXSTRTM								0x00000004
#define	CSR4_JABM									0x00000001
#define	CSR4_MASK_ALL_INTS							CSR4_MFCOM | CSR4_RCVCCOM | CSR4_TXSTRTM | CSR4_JABM

#define	CSR5_LTINTE									0x00004000
#define	CSR5_SINTE									0x00000400
#define	CSR5_SLPINTE								0x00000100
#define	CSR5_EXDINTE								0x00000040
#define	CSR5_MPINTE									0x00000008
#define	CSR5_MASK_ALL_INTS							CSR5_LTINTE | CSR5_SINTE | CSR5_SLPINTE | CSR5_EXDINTE | CSR5_MPINTE

#define	CSR58_SSIZE32								0x00000100
#define	CSR58_SW_STYLE_PCNET_PCI_II_CONTROLLER		0x00000003

#define	CSR15_PROMISCUOUS_MODE						0x00008000

#define	BCR2_ASEL									0x00000001
#define	BCR2_INTLEVEL								0x00000080



/////////////////////////////////////////////////////////////////////////////////
//	Misc handy defines...
//

#define WriteByte(Offset, Value)    WRITE_PORT_UCHAR((PUCHAR)(pbEthernetBase+Offset), Value)
#define ReadByte(Offset)            READ_PORT_UCHAR((PUCHAR)(pbEthernetBase+Offset))

#define WriteWord(Offset, Value)    WRITE_PORT_USHORT((PUSHORT)(pbEthernetBase+Offset), Value)
#define ReadWord(Offset)            READ_PORT_USHORT((PUSHORT)(pbEthernetBase+Offset))

#define WriteDword(Offset, Value)   WRITE_PORT_ULONG((PULONG)(pbEthernetBase+Offset), Value)
#define ReadDword(Offset)           READ_PORT_ULONG((PULONG)(pbEthernetBase+Offset))

/////////////////////////////////////////////////////////////////////////////////
//	Register Address Port, as the base for indirect addressing when accessing:
//	-	CSR (Control and Status Register) via RDP (Register Data Port).
//	-	BCR (Bus Control Registers) via BDP (BCR Data Port).
//

/*
#define	RAP	((PULONG) (pbEthernetBase + 0x14))
#define	RDP	((PULONG) (pbEthernetBase + 0x10))
#define	BDP	((PULONG) (pbEthernetBase + 0x1C))
*/

#define	RAP	((PULONG) (pbEthernetBase + 0x12))
#define	RDP	((PULONG) (pbEthernetBase + 0x10))
#define	BDP	((PULONG) (pbEthernetBase + 0x16))

/*
#define	RAPb4	*((volatile PWORD) (pbEthernetBase + 0x14))
#define	RDPb4	*((volatile PWORD) (pbEthernetBase + 0x10))
#define	BDPb4	*((volatile PWORD) (pbEthernetBase + 0x16))
*/


/////////////////////////////////////////////////////////////////////////////////
//	Receive Descriptor
//

#define	RMD2_MUST_AND	0xFFFF0FFF
typedef union _RX_RMD2__
{
	struct 
	{
		DWORD	MCNT		: 12;		//	[0]  Message Byte Count
		DWORD	_Reserved_	: 4;		//	[12] Reserved...MUST BE ZERO
		DWORD	RPC			: 8;		//	[16] Runt Packet Count		
		DWORD	RCC			: 8;		//	[24] Receive Collision count
	};

	DWORD	dwReg;

} RX_RMD2;



#define		RMD1_MUST_AND	0xFFF0FFFF
#define		RMD1_MUST_OR	0x0000F000

#define		OWN_HOST		0x80000000
#define		RMD1_DEFAULT	OWN_HOST | RMD1_MUST_OR

typedef union _RX_RMD1__
{
	struct
	{
		DWORD	BCNT		: 12;		//	[0]	 Buffer Byte Count (Buffer length).
		DWORD	_ResONES	: 4;		//	[12] Reserved...MUST BE ONE
		DWORD	_ResZEROS	: 4;		//	[16] Reserved...MUST BE ZERO
		DWORD	BAM			: 1;		//	[20] BROADCAST packet.	
		DWORD	LAFM		: 1;		//	[21] Logical Address Filter Match.	
		DWORD	PAM			: 1;		//	[22] Physical Address Filter Match.
		DWORD	BPE			: 1;		//	[23] Bus Parity Error.
		DWORD	ENP			: 1;		//	[24] End of packet.
		DWORD	STP			: 1;		//	[25] Start of Packet.
		DWORD	BUFF		: 1;		//	[26] Buffer Error...i.e. indication of Descriptor buffer overflow...
		DWORD	CRC			: 1;		//	[27] CRC error.
		DWORD	OFLO		: 1;		//	[28] Receiver Over Flow.
		DWORD	FRAM		: 1;		//	[29] Framing error.
		DWORD	ERR			: 1;		//	[30] There is an ERROR.
		DWORD	OWN			: 1;		//	[31] OWN bit (Host = 0 ; AMD = 1)
	};

	DWORD	dwReg;

} RX_RMD1;


typedef struct _tagRxDescriptor__
{
	RX_RMD2		RMD2;
	RX_RMD1		RMD1;
	DWORD		RBADR;	
	DWORD		Reserved;

} RX_DESCRIPTOR_FORMAT, *PRX_DESCRIPTOR_FORMAT;



/////////////////////////////////////////////////////////////////////////////////
//	Transmit Descriptor
//

typedef union _TX_TMD1__
{
	struct 
	{
		DWORD	BCNT		: 12;		//	[0]  Buffer Byte Count, the usable length of the buffer pointed by this desc.
		DWORD	ALL_ONES	: 4;		//	[12] MUST BE ONEs...
		DWORD	RES			: 7;		//	[16] Reserved location.
		DWORD	BPE			: 1;		//	[23] Bus Parity Error, set by AMD
		DWORD	ENP			: 1;		//	[24] End Of Packet.
		DWORD	STP			: 1;		//	[25] Start of Packet.
		DWORD	DEF			: 1;		//	[26] Defered, set by AMD cleared by host.
		DWORD	ONE			: 1;		//	[27] Exactly one buffer used.
		DWORD	MORE		: 1;		//	[28] Set by AMD if last buffer transmitted interrupt needed.
		DWORD	ADD_FCS		: 1;		//	[29] Tell AMD to generate FCS
		DWORD	ERR			: 1;		//	[30] Error occured during transmission...
		DWORD	OWN			: 1;		//	[31] OWN bit (Host = 0 ; AMD = 1)
		
	};

	DWORD	dwReg;

} TX_TMD1;


typedef	union _TX_TMD2__
{
	struct
	{
		DWORD	TRC			: 4;		//	[0]  Transmit Retry Count
		DWORD	RES			: 12;		//	[4]  Reserved location.	
		DWORD	TDR			: 10;		//	[16] Time Domain Reflectometer... Not useful !!!
		DWORD	RTRY		: 1;		//	[26] Retry Error.
		DWORD	LCAR		: 1;		//	[27] Lost of Carrier.
		DWORD	LCOL		: 1;		//	[28] Late Collision.
		DWORD	EXDEF		: 1;		//	[29] Excessive Deferral.
		DWORD	UFLO		: 1;		//	[30] Underflow error.
		DWORD	BUFF		: 1;		//	[31] Buffer Error.
	};

	DWORD	dwReg;

} TX_TMD2;


typedef struct _tagTxDescriptor__
{
	TX_TMD2		TMD2;
	TX_TMD1		TMD1;
	DWORD		TBADR;	
	DWORD		Reserved;

} TX_DESCRIPTOR_FORMAT, *PTX_DESCRIPTOR_FORMAT;


/////////////////////////////////////////////////////////////////////////////////
//	Exported funtions to caller
//
//

BOOL	AM79C970Init (BYTE *pbBaseAddress, ULONG	dwMemOffset, USHORT MacAddr[3]);
void	AM79C970EnableInts (void);
void	AM79C970DisableInts (void);
DWORD	AM79C970GetPendingInts (void);
UINT16	AM79C970GetFrame (BYTE *pbData, UINT16 *pwLength, BOOL	bBootLoaderCall);
UINT16	AM79C970SendFrame (BYTE *pbData, DWORD dwLength);
void	AM79C970InitTxDescriptor (DWORD TxHead, DWORD TxBuffSize, BOOL bVirt);
void	AM79C970InitRxDescriptor (DWORD RxHead, DWORD RxBuffSize, BOOL bVirt);
DWORD	AM79C970QueryBufferSize (void);
DWORD	AM79C970QueryDescriptorSize (void);
                          
#endif	// _AMD_79C970_HARDWARE_
