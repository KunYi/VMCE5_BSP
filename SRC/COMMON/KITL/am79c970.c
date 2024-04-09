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
	Plain Vanilla Routines for AMD Am79c970a NIC.
	This mainly supports initialization and Sending/Receiving Ethernet packets
	as well as interrupt handler.
	Minimum error handling is provided here...
    
Functions:
	
Notes: 
	Code is meant to be used for ethernet download and debugging only and 
	is expected to run in Kernel Mode...

    
--*/

#include <windows.h>
#include <halether.h>
#include <ceddk.h>
#include "am79c970.h"


/////////////////////////////////////////////////////////////////////////////////
//	External Functions that must be there...
//

#define localDEBUGMSG EdbgOutputDebugString
//extern	void	localDEBUGMSG(const unsigned char *sz, ...);
extern	DWORD	OEMEthGetMSecs (void);


/////////////////////////////////////////////////////////////////////////////////
//	Global Variables and defines...
//

#define		MAX_BUFFER_SIZE			1536	//	12*128(=max cache line size)

PBYTE		pbEthernetBase;					//	Ethernet IO Base Address...

DWORD		dwTRANSMIT_DESCRIPTORS_HEAD;	//	Start of TX descriptors.
DWORD		dwTRANSMIT_BUFFER_START;		//	Start of TX buffer linked to descriptors.
DWORD		dwTRANSMIT_RING_SIZE;			//	Size of the entire TX ring.

DWORD		dwRECEIVE_DESCRIPTORS_HEAD;		//	Start of RX descriptors.
DWORD		dwRECEIVE_BUFFER_START;			//	Start of RX buffer linked to descriptors.
DWORD		dwRECEIVE_RING_SIZE;			//	Size of the entire RX ring.


static		DWORD					dwMEM_OFFSET;			//	When NIC and CPU not having same offset to memory...

volatile	PRX_DESCRIPTOR_FORMAT	pCurrentRxDesc;			//	pointer to current RX Descriptor that Rx may use.
volatile	PRX_DESCRIPTOR_FORMAT	pLastRxDesc;			//	pointer to Last RX descriptor.

volatile	PTX_DESCRIPTOR_FORMAT	pCurrentTxDesc;			//	pointer to current TX Descriptor that Tx may use.
volatile	PTX_DESCRIPTOR_FORMAT	pLastTxDesc;			//	pointer to Last TX descriptor.


/////////////////////////////////////////////////////////////////////////////////
//	Forward decl...
//
void	DumpOneTxDescriptor (PTX_DESCRIPTOR_FORMAT	 pTxDesc);
void	DumpOneRxDescriptor (PRX_DESCRIPTOR_FORMAT	 pRxDesc);
void	DumpTxDescriptors ();
void	DumpRxDescriptors ();
void	DumpMemory (PBYTE pSource, DWORD dwLength);
void	DumpSenderAddr (PBYTE pSource);



/////////////////////////////////////////////////////////////////////////////////
//	Misc helper functions...
//

#define		SECOND_COMP(x)		((long) (0-(DWORD)x))
//#define		TO_REAL(Addr)		((Addr & 0x1fffffff) + dwMEM_OFFSET)
//#define		TO_VIRT(Addr)		((Addr | 0xA0000000) - dwMEM_OFFSET)
#define		TO_REAL(Addr)		(Addr)
#define		TO_VIRT(Addr)		(Addr)


void SleepLoop (DWORD dwCounter)
{
	//	Simply loop...
	while (dwCounter--)
		;
}	// SleepLoop()


void WriteCSR (DWORD dwIndex, DWORD dwValue)
{
	//localDEBUGMSG (">>> [%d] <-- 0x%x \r\n", dwIndex, dwValue);
	WRITE_PORT_ULONG(RAP, dwIndex);
	WRITE_PORT_ULONG(RDP, dwValue);
}

DWORD ReadCSR (DWORD dwIndex)
{
    DWORD dwRAP;

    WRITE_PORT_ULONG(RAP, dwIndex);
    dwRAP = READ_PORT_ULONG(RDP);
    return dwRAP;
}

void WriteBCR (DWORD dwIndex, DWORD dwValue)
{
    WRITE_PORT_ULONG(RAP, dwIndex);
    WRITE_PORT_ULONG(BDP, dwValue);
}

DWORD ReadBCR (DWORD dwIndex)
{
    DWORD dwBDP;
    
    WRITE_PORT_ULONG(RAP, dwIndex);
    dwBDP = READ_PORT_ULONG(BDP);
    return dwBDP;
}


PTX_DESCRIPTOR_FORMAT GetNextTxDesc (PTX_DESCRIPTOR_FORMAT pGivenTxDesc)
{
	if (pGivenTxDesc == pLastTxDesc)
		return (PTX_DESCRIPTOR_FORMAT) dwTRANSMIT_DESCRIPTORS_HEAD;		
	else
		return (PTX_DESCRIPTOR_FORMAT) (pGivenTxDesc + 1);
}


PRX_DESCRIPTOR_FORMAT GetNextRxDesc (PRX_DESCRIPTOR_FORMAT pGivenRxDesc)
{
	if (pGivenRxDesc == pLastRxDesc)
		return (PRX_DESCRIPTOR_FORMAT) dwRECEIVE_DESCRIPTORS_HEAD;		
	else
		return (PRX_DESCRIPTOR_FORMAT) (pGivenRxDesc + 1);
}



/////////////////////////////////////////////////////////////////////////////////
//	InitTxDescriptor initializes a given descriptor pointed to by pTxDesc to
//	use buffer pointed to by pBuffer...
//

void InitTxDescriptor (PTX_DESCRIPTOR_FORMAT pTxDesc, DWORD	pBuffer)
{
	//localDEBUGMSG ("Address: 0x%x --- Buffer 0x%x \r\n", pTxDesc, pBuffer);
								
	pTxDesc->TMD2.dwReg		= 0x00;							//	AMD controls the contents here...	
	
	pTxDesc->TMD1.dwReg		= 0x00;							//	Important one is the OWN bit settting.
	pTxDesc->TMD1.ALL_ONES  = 0xf;							//	And the MUST BE ONEs field.
	pTxDesc->TMD1.BCNT		= SECOND_COMP(MAX_BUFFER_SIZE);
	
	if (pBuffer)											//	Only point to buffer if pBuffer is avail...
		pTxDesc->TBADR		= TO_REAL(pBuffer);				//	TX buffer for this descriptor...

}	// InitTxDescriptor()


/////////////////////////////////////////////////////////////////////////////////
//	InitTxDescriptor initializes a given descriptor pointed to by pRxDesc to
//	use buffer pointed to by pBuffer...
//

void InitRxDescriptor (PRX_DESCRIPTOR_FORMAT pRxDesc, DWORD	pBuffer)
{
	//localDEBUGMSG ("Address: 0x%x --- Buffer 0x%x \r\n", pRxDesc, pBuffer);

	pRxDesc->RMD2.dwReg = 0x00;								//	AMD controls the fields in here...
	
	pRxDesc->RMD1.dwReg = RMD1_DEFAULT;						//	The goodies...
	pRxDesc->RMD1.BCNT	= SECOND_COMP(MAX_BUFFER_SIZE);		//	Main things are the size of buffer + OWN bit.
	
	if (pBuffer)
		pRxDesc->RBADR		= TO_REAL(pBuffer);					//	Rx buffer for this descriptor...

}	// InitRxDescriptor()


/////////////////////////////////////////////////////////////////////////////////
//	AM789C970Init()
//	Initializes the NIC.
//	pbBaseAddress = IO base address assigned to the NIC.
//	dwMemOffset	  = Can be used if the common memory (for descriptors and mem)
//					is not at the same offset as seen between host and NIC.
//	MacAddr		  = Contains the MAC address read from hardware upon successful
//					initialization.
//
//	Return value:
//		TRUE when NIC initialized successfully, FALSE otherwise.
//
//
//	Initialization procedure:
//	1.	BCR20 (or CSR58) bit 8 (SSIZE32) set to 1 for 32 bit accesses.	
//	2.	Use Alternative method for Initialization (Appendix C of am79c970a spec).
//	3.	Setup Transmit and Receive Descriptor Rings.
//
//	x.	Finally... Set INIT bit in CSR0 to get am789c970 to start reading the 
//		initialization block.
//


BOOL	AM79C970Init(BYTE *pbBaseAddress, ULONG	dwMemOffset, USHORT MacAddr[3])
{
	DWORD	i;	

    if (MacAddr == NULL) return FALSE;
    
	localDEBUGMSG ("AM79C970Init:: Init using i/o address: 0x%x - MEM Offset = 0x%x\r\n", 
		pbBaseAddress, dwMemOffset);

	pbEthernetBase = pbBaseAddress;	
	dwMEM_OFFSET   = dwMemOffset;


	/////////////////////////////////////////////////////////////////////////////
	//	Make sure all the tx and rx process stopped.
	//	I may get to this code because of jumping to reset vector from the 
	//	bootloader flash download...
	//	
	WriteCSR (0, 0x04);		
	SleepLoop(10000);



	/////////////////////////////////////////////////////////////////////////////
	//	The very first step is to soft reset the controller and invoke
	//	DWIO (DWord IO) mode (accomplished by writing 32 bit to RDP).
	//

	/////////////////////////////////////////////////////////////////////////////
	//	This causes the internal reset, and AMD PCI controller blocks further 
	//	accesses until its internal reset is done (around 1 uS).
	ReadWord (RESET_REGISTER);	
	WriteDword	(0x10, 0x00);
	SleepLoop(10000000);


	/////////////////////////////////////////////////////////////////////////////
	//	We are in business... First turn is to fix the MAC address required by 
	//	caller...
	//
	if( NULL != MacAddr ) {
		for (i = 0 ; i < 3 ; i++)	
			MacAddr[i] = ReadWord(i*2);	
	}

	
	localDEBUGMSG ("+--------------------- BEFORE -------------------------+\r\n");
	localDEBUGMSG ("CSR0 == 0x%x \r\n", ReadCSR(0));			
	localDEBUGMSG ("CSR04 == 0x%x \r\n", ReadCSR(4));		
	localDEBUGMSG ("CSR15 == 0x%x \r\n", ReadCSR(15));		
	localDEBUGMSG ("CSR58 == 0x%x \r\n", ReadCSR(58));		
	localDEBUGMSG ("BCR02 == 0x%x \r\n", ReadBCR(2));		
	localDEBUGMSG ("BCR9  == 0x%x \r\n", ReadBCR(9));
	

	/////////////////////////////////////////////////////////////////////////////
	//	Set the software style correctly then proceed with initialization 
	//	explained in appendix c of am79c970a manual.	
	//

	WriteCSR (58, CSR58_SSIZE32 | CSR58_SW_STYLE_PCNET_PCI_II_CONTROLLER);		// Software style.	
	WriteCSR (4, ReadCSR(4) | CSR4_DMA_PLUS | CSR4_APAD_XMIT | CSR4_TXSTRTM);	// Must be set for PCNET_PCI_II setting.		
	WriteBCR (2, ReadBCR(2) | BCR2_ASEL);										// Use auto select feature.

	WriteCSR (8, 0x00);		/////////////////////////////////////////////////////
	WriteCSR (9, 0x00);		// At this point we don't care about Logical adddr
	WriteCSR (10, 0x00);	// Filtering
	WriteCSR (11, 0x00);	//

								/////////////////////////////////////////////////////
	WriteCSR (12, MacAddr[0]);	//	Physical Address [15:00]
	WriteCSR (13, MacAddr[1]);	//	Physical Address [31:16]
	WriteCSR (14, MacAddr[2]);	//	Physical Address [47:32]

	WriteCSR (15, 0x00);		//	Turn off Promiscuous mode...

	WriteCSR (24, TO_REAL(dwRECEIVE_DESCRIPTORS_HEAD) & 0x0000ffff);			// RX descriptor starts...
	WriteCSR (25, (TO_REAL(dwRECEIVE_DESCRIPTORS_HEAD) & 0xffff0000) >> 16);

	WriteCSR (30, TO_REAL(dwTRANSMIT_DESCRIPTORS_HEAD) & 0x0000ffff);			// TX descriptor starts...
	WriteCSR (31, (TO_REAL(dwTRANSMIT_DESCRIPTORS_HEAD) & 0xffff0000) >> 16);

	WriteCSR (47, (DWORD) SECOND_COMP(0));								// Polling interval, use default...1.966ms
	WriteCSR (76, (DWORD) SECOND_COMP(dwRECEIVE_RING_SIZE));			// RX Ring Size...
	WriteCSR (78, (DWORD) SECOND_COMP(dwTRANSMIT_RING_SIZE));			// TX Ring Size...

	WriteBCR (9, ReadBCR(9) & 0xfffffffe);				// Make sure it is half duplex...
	WriteCSR (3, ReadCSR(3) | CSR3_MASK_ALL_INTS);		// Start off with all interrupt mask...EnableInts() will handle which one to turn on.


	/////////////////////////////////////////////////////////////////////////////
	//	Lets read 'em back for fun... :-)
	//	
	localDEBUGMSG ("+--------------------- AFTERWARDS -------------------------+\r\n");
	localDEBUGMSG ("CSR0 == 0x%x \r\n", ReadCSR(0));		
	localDEBUGMSG ("CSR3  == 0x%x \r\n", ReadCSR(3));
	localDEBUGMSG ("CSR04 == 0x%x \r\n", ReadCSR(4));		
	localDEBUGMSG ("CSR12 == 0x%x \r\n", ReadCSR(12));
	localDEBUGMSG ("CSR13 == 0x%x \r\n", ReadCSR(13));
	localDEBUGMSG ("CSR14 == 0x%x \r\n", ReadCSR(14));
	localDEBUGMSG ("CSR15 == 0x%x \r\n", ReadCSR(15));		
	localDEBUGMSG ("CSR24 == 0x%x \r\n", ReadCSR(24));		
	localDEBUGMSG ("CSR25 == 0x%x \r\n", ReadCSR(25));		
	localDEBUGMSG ("CSR30 == 0x%x \r\n", ReadCSR(30));		
	localDEBUGMSG ("CSR31 == 0x%x \r\n", ReadCSR(31));		
	localDEBUGMSG ("CSR47 == 0x%x \r\n", ReadCSR(47));
	localDEBUGMSG ("CSR58 == 0x%x \r\n", ReadCSR(58));		
	localDEBUGMSG ("CSR76 == 0x%x \r\n", ReadCSR(76));
	localDEBUGMSG ("CSR78 == 0x%x \r\n", ReadCSR(78));	
	localDEBUGMSG ("BCR02 == 0x%x \r\n", ReadBCR(2));		
	localDEBUGMSG ("BCR9  == 0x%x \r\n", ReadBCR(9));
	
	

	/////////////////////////////////////////////////////////////////////////////
	//	Initialize TX Descriptors...
	//
	pCurrentTxDesc = (PTX_DESCRIPTOR_FORMAT) dwTRANSMIT_DESCRIPTORS_HEAD;		
	pLastTxDesc    = (PTX_DESCRIPTOR_FORMAT) (pCurrentTxDesc + dwTRANSMIT_RING_SIZE - 1);

	for (i = 0 ; i < dwTRANSMIT_RING_SIZE ; i++)
	{
		InitTxDescriptor ((PTX_DESCRIPTOR_FORMAT) (pCurrentTxDesc + i), 
						  (DWORD) (dwTRANSMIT_BUFFER_START + i * MAX_BUFFER_SIZE));
	}
	//DumpTxDescriptors();


	/////////////////////////////////////////////////////////////////////////////
	//	Initialize RX Descriptors...
	//
	pCurrentRxDesc = (PRX_DESCRIPTOR_FORMAT) dwRECEIVE_DESCRIPTORS_HEAD;
	pLastRxDesc    = (PRX_DESCRIPTOR_FORMAT) (pCurrentRxDesc + dwRECEIVE_RING_SIZE - 1) ;

	for (i = 0 ; i < dwRECEIVE_RING_SIZE ; i++)
	{
		InitRxDescriptor ((PRX_DESCRIPTOR_FORMAT) (pCurrentRxDesc + i), 
						  (DWORD) (dwRECEIVE_BUFFER_START + i * MAX_BUFFER_SIZE));
	}
	//DumpRxDescriptors();


	/////////////////////////////////////////////////////////////////////////////
	//	All aboard... Let the engine roar...	
	//
	
	WriteCSR (0, (ReadCSR(0) | 0x02) & ~0x04);	// Turn on START bit...and turn off STOP bit.

/*
	DumpOneRxDescriptor (pCurrentRxDesc);
	while (pCurrentRxDesc->RMD1.OWN)
		;
	localDEBUGMSG ("Data received...%d bytes\r\n", pCurrentRxDesc->RMD2.MCNT);
	DumpOneRxDescriptor (pCurrentRxDesc);
	DumpOneRxDescriptor (pCurrentRxDesc + 1);
	DumpMemory ((PUCHAR) (TO_VIRT(pCurrentRxDesc->RBADR)), pCurrentRxDesc->RMD2.MCNT);		
	for (;;)
		;
*/

	SleepLoop(5000);
	localDEBUGMSG ("CSR0 == 0x%x \r\n", ReadCSR(0));		

	return TRUE;
}	// AM79C970Init()


/////////////////////////////////////////////////////////////////////////////////
//	AM79C970EnableInts()
//	For Ether Debug, we only need Receive Interrupt.
//	Hence, turn on IENA and mask all other interrupts except Receive Interrupt.
//
void	AM79C970EnableInts (void)
{
	/////////////////////////////////////////////////////////////////////////////
	//	First, make sure all interrupts are mask, except the receive interrupt...
	//

	WriteCSR (3, ReadCSR(3) | CSR3_MASK_ALL_INTS);
	WriteCSR (4, ReadCSR(4) | CSR4_MASK_ALL_INTS);
	WriteCSR (5, ReadCSR(5) | CSR5_MASK_ALL_INTS);	
	WriteCSR (3, ReadCSR(3) & ~CSR3_RINTM);

	
	/////////////////////////////////////////////////////////////////////////////
	//	Then enable IENA in CSR0, the father of all interrupts !!!
	//

	WriteCSR (0, ReadCSR(0) | CSR0_IENA);		

		
	{
		//RETAILMSG (1, (TEXT("CSR0 = 0x%x \r\n"), ReadCSR(0)));		
		//RETAILMSG (1, (TEXT("CSR3 = 0x%x \r\n"), ReadCSR(3)));				
		//RETAILMSG (1, (TEXT("CSR4 = 0x%x \r\n"), ReadCSR(4)));
		//RETAILMSG (1, (TEXT("CSR5 = 0x%x \r\n"), ReadCSR(5)));
		//RETAILMSG (1, (TEXT("BCR2 = 0x%x \r\n\r\n\r\n"), ReadBCR(2)));
		
	}
		


}	// AM79C970EnableInts()


/////////////////////////////////////////////////////////////////////////////////
//	AM79C970DisableInts()
//	Simply turn off CSR0_IOENA...
//
void	AM79C970DisableInts(void)
{
	WriteCSR (0, (ReadCSR(0) & ~CSR0_IENA));

}	// AM79C970DisableInts()


/////////////////////////////////////////////////////////////////////////////////
//	
//
DWORD	AM79C970GetPendingInts(void)
{	
	//RETAILMSG (1, (TEXT("GetPending: CSR0 = 0x%x \r\n"), ReadCSR(0)));

	WriteCSR (0, ReadCSR(0) | CSR0_RINT);
	return INTR_TYPE_RX;

}	// AM79C970EnableInts()




/////////////////////////////////////////////////////////////////////////////////
//	AM79C970GetFrame()
//
//	This routine is used to find out if a frame has been 
//	received.  If there are no frames in the RX FIFO, the routine will return 0.  
//	If there is a frame that was received correctly, it will be stored in pwData, 
//	otherwise it will be discarded.  
//

void SkipAllErrorDescriptors(void)
{
	/////////////////////////////////////////////////////////////////////////
	//	FM-FM-FM Should investigate properly what causes this, instead of
	//	just blowing them away...
	//	ASSUMING that OWN will be set even for ERRORNEOUS packets...
	//	
	while (pCurrentRxDesc->RMD1.OWN && pCurrentRxDesc->RMD1.ERR)
	{
		localDEBUGMSG ("Error Descriptor: %d\r\n", 
			pCurrentRxDesc - (PRX_DESCRIPTOR_FORMAT) dwRECEIVE_DESCRIPTORS_HEAD);
		InitRxDescriptor (pCurrentRxDesc, 0);
		pCurrentRxDesc = GetNextRxDesc (pCurrentRxDesc);
	}
}	// SkipAllErrorDescriptors()



BOOL Broadcast(PBYTE pHeader)
{
	int	i = 0;
	while (1)
	{
		if (pHeader[i++] != 0xff)
			break;
		if (i == 6)
			return TRUE;		
	}
	
	return FALSE;
}	// Broadcast()



UINT16	AM79C970GetFrame(BYTE *pbData, UINT16 *pwLength, BOOL	bBootLoaderCall)
{
	
	BOOL	bDone			= FALSE;		//	When we loop for ENP.
	UINT16	dwOffset		= 0;			//	Offset to User's pbData...
	UINT16	dwBytesToCopy	= 0;
	BOOL	bTossedPacket   = FALSE;
	PBYTE	pHeader;
	

	*pwLength = 0;

	/////////////////////////////////////////////////////////////////////////////
	//	Bail out immediately if there is no data avail...
	//
	if (pCurrentRxDesc->RMD1.OWN)
		return 0;

	/////////////////////////////////////////////////////////////////////////////
	//	Okay, at least we must have all or part of data available...
	//	ASSUMING AMD is doing the right thing, I won't detect STP but just the
	//	ENP.   If error happens in between packets, whatever received will be 
	//	tossed away.
	//	[stjong 11/14/98]
	//	Alright, it is a WRONG assumption, if STP is set without ENP, it means
	//	the whole buffer is filled, eventhough MCNT reports zero !!!
	//	What's more, the last packet contains the exact total number of packets
	//	received... (what a bummer !!!)
	//
	while (!bDone)
	{
		//localDEBUGMSG ("Using RX Descriptor Number: %d \r\n", 
		//	(pCurrentRxDesc - (PRX_DESCRIPTOR_FORMAT) dwRECEIVE_DESCRIPTORS_HEAD));

		/////////////////////////////////////////////////////////////////////////
		//	If it is error packets, throw away until we get to descriptor not 
		//	yet used.   And return zero to user...
		//
		if (pCurrentRxDesc->RMD1.ERR)
		{				
			SkipAllErrorDescriptors();
			return 0;
		}		


		/////////////////////////////////////////////////////////////////////////
		//	So here is the logic,
		//	If it is ENP (End Of Packet) and STP (Start Of Packet), 
		//	number of bytes to copy is the number of bytes pointed to by MCNT.
		//	If it is NOT end of packet, then MAX_BUFFER_SIZE takes effect.
		//	For multi buffer packet (ONLY ENP set and NOT STP), we just need to 
		//	copy whatever less to make up MCNT.
		//

		if (!bBootLoaderCall && pCurrentRxDesc->RMD1.STP)		
		{
			/////////////////////////////////////////////////////////////////////
			//	Determine if we need to toss this packet away...
			//	Toss this away if it is non ARP broadcast...
			//
			pHeader = (PVOID)TO_VIRT(pCurrentRxDesc->RBADR);
			if (Broadcast(pHeader))
			{
				if (pHeader[12] != 0x08 && pHeader[13] != 0x06)
				{
					/////////////////////////////////////////////////////////////
					//	This packet is tossed...
					//					
					bTossedPacket = TRUE;				
				}
			}
		}


		if (pCurrentRxDesc->RMD1.ENP)
		{
			bDone = TRUE;
			if (pCurrentRxDesc->RMD1.STP)
				dwBytesToCopy = (UINT16) pCurrentRxDesc->RMD2.MCNT;
			else
				dwBytesToCopy = (UINT16) (pCurrentRxDesc->RMD2.MCNT - dwOffset);
		}
		else
			dwBytesToCopy = MAX_BUFFER_SIZE;

		if (bTossedPacket)
			dwBytesToCopy = 0x00;

		// localDEBUGMSG ("BYTES COPIED : %d\r\n", dwBytesToCopy);

		memcpy (pbData + dwOffset, 
				(PVOID) TO_VIRT(pCurrentRxDesc->RBADR),
				dwBytesToCopy);

		dwOffset += dwBytesToCopy;								//	Increase what we have copied so far.
		
		InitRxDescriptor(pCurrentRxDesc, 0);					//	Initialize the used RX Descriptor.	
		pCurrentRxDesc = GetNextRxDesc (pCurrentRxDesc);		//	Forward to next descriptor...
		

		/////////////////////////////////////////////////////////////////////////
		//	Now, if I am not done yet and the next packet is now owned by me,
		//	just loop till it is owned by me...
		//	In interrupt mode, it should be Okay, because by the time RX interrupt
		//	received, I am guaranteed to have received the entirety of the packet.
		//	For Bootloader download, I don't care, it is single task and I will
		//	have nothing else to do except waiting for the data...
		//	But becareful not to poll too fast, as it affects the DMA transfer
		//	that must be going on...
		//
		
		if (!bDone)
		{
			while (pCurrentRxDesc->RMD1.OWN)
			{
				localDEBUGMSG (">>> Waiting for descriptor %d to complete packet.\r\n",
					pCurrentRxDesc - (PRX_DESCRIPTOR_FORMAT) dwRECEIVE_DESCRIPTORS_HEAD);
				SleepLoop(10000000);
			}					
		}
	}

	// DumpSenderAddr (pbData);

	if (dwOffset >= 4) 
		dwOffset -=4; // Takeout CRC.
	*pwLength = (UINT16)dwOffset;
	return (UINT16)dwOffset;
}	// AM79C970GetFrame()



/////////////////////////////////////////////////////////////////////////////////
//	AM79C970SendFrame()
//
//	This routine should be called with a pointer to the ethernet frame data.  
//	It is the caller's responsibility to fill in all information including the 
//	destination and source addresses and the frame type.  
//	The length parameter gives the number of bytes in the ethernet frame.
//	The routine will return immediately regardless of whether transmission 
//	has successfully been performed.
//	

UINT16	AM79C970SendFrame(BYTE *pbData, DWORD dwLength)
{
	UINT		dwTotalCopied = 0;
	UINT		dwBytesToCopy = 0;
	
	volatile	PTX_DESCRIPTOR_FORMAT	pFirstTxDesc = pCurrentTxDesc;
	volatile	PTX_DESCRIPTOR_FORMAT	pNextTxDesc;


	// localDEBUGMSG ("\r\n*** Sending: %d bytes.\r\n", dwLength);	

	/////////////////////////////////////////////////////////////////////////////
	//	Loop until all data has been copied to the descriptor's buffer.
	//	Depending on buffer size, all data may fit into one buffer or span 
	//	through multiple descriptors' buffers.
	//	

	while (dwTotalCopied < dwLength)
	{	
		localDEBUGMSG ("Using TX Descriptor Number: %d \r\n", 
			(pCurrentTxDesc - (PTX_DESCRIPTOR_FORMAT) dwTRANSMIT_DESCRIPTORS_HEAD));	


		/////////////////////////////////////////////////////////////////////////
		//	Figure out how many bytes to copy to descriptor's buffer...
		//	
		if ((dwLength - dwTotalCopied) < MAX_BUFFER_SIZE)			
			dwBytesToCopy = (dwLength - dwTotalCopied);
		else
			dwBytesToCopy = MAX_BUFFER_SIZE;

		memcpy ((PVOID)TO_VIRT(pCurrentTxDesc->TBADR),			
				 pbData + dwTotalCopied, 
				 dwBytesToCopy);		

		dwTotalCopied += dwBytesToCopy;

		/////////////////////////////////////////////////////////////////////////
		//	See if I can proceed to next descriptor, if not do something to 
		//	speed up the sending (error or whatever).
		//	If so, proceed to the next descriptor...
		//	

		pNextTxDesc = GetNextTxDesc (pCurrentTxDesc);

		while (pNextTxDesc->TMD1.OWN)
		{
			/////////////////////////////////////////////////////////////////////
			//	Need to do something here to figure out why sending is halted.
			//	Seems like we have loop back one round and data is still not
			//	yet sent... FM-FM-FM-FM			
			//	For now just hang on here till I get the next descriptor...
			//
//			localDEBUGMSG ("Waiting for TX descriptor number = %d --- Last Descriptor = %d\r\n",
//				pNextTxDesc - (PTX_DESCRIPTOR_FORMAT) dwTRANSMIT_DESCRIPTORS_HEAD,
//				pLastTxDesc - (PTX_DESCRIPTOR_FORMAT) dwTRANSMIT_DESCRIPTORS_HEAD);
			SleepLoop(10000000);
		}


		/////////////////////////////////////////////////////////////////////////
		//	If we get here means that we have secured next descriptor...
		//	Let's proceed...
		//	But before that, let's update all other fields in the descriptors.
		//	The only field we can't update is the OWN bit for the first descriptor..
		//
			
		if (pCurrentTxDesc == pFirstTxDesc)		
			pCurrentTxDesc->TMD1.STP = 0x01;
		else
			pCurrentTxDesc->TMD1.OWN = 0x01;

		if (dwBytesToCopy < MAX_BUFFER_SIZE)
			pCurrentTxDesc->TMD1.ENP = 0x01;
		

		pCurrentTxDesc->TMD1.BCNT = SECOND_COMP(dwBytesToCopy);
		pCurrentTxDesc = pNextTxDesc;

		/////////////////////////////////////////////////////////////////////////
		//	Make sure we set the descriptor to a known state...
		//
		InitTxDescriptor (pCurrentTxDesc, 0);

	}

	/////////////////////////////////////////////////////////////////////////////
	//	Now that all the data been copied to the buffer, it's BOOGIE time...
	//	
	pFirstTxDesc->TMD1.OWN = 0x01;	
	WriteCSR (0, ReadCSR(0) | CSR0_TDMD);		


	/////////////////////////////////////////////////////////////////////////////
	//	Oh well, the previous libraries need to return zero when sending 
	//	successfully...
	//
	return 0; 

}	// AM79C970SendFrame()


/////////////////////////////////////////////////////////////////////////////////
//	AM79C970InitTxDescriptor()
//
//	Caller tells us where the descriptors and buffers should live.   
//	One descriptor should have one buffer.   
//	Hence the dwTotalTx applies to both decriptors and buffers.
//	Furthermore, the size of buffer is fixed at MAX_BUFFER_SIZE.
//	It is caller's responsibility to allocate enough memory...
//	Note:   Caller can use AM79C970QueryBufferSize() to find out the size of buffer 
//			used for each buffer and hence calculate the Total Buffer that can be 
//			allocated...	
//			AM79C970QueryDescriptorSize() will return size of descriptor...
//	Input:	
//	-	TxHead		=	Location where TX descriptors start (Physical or Virtual Address).
//	-	TxBuffSize  =	Size of buffer allocated by caller.
//	-	bVirt		=	Whether or not the address is Physical or Virtual.
//						Bootloader and Ethdbg run in Kernel mode and hence will have this 
//						set to FALSE.
//						However, NDIS mini driver may run in user mode and hence this 
//						library MAY need to use virtual address to access the 
//						descriptors and buffers... 
//						Provided the virtual address, the library will figure out the 
//						actual physical address...
//
//	WARNING:	IT IS CALLER RESPONSIBILITY TO GIVE PROPER DATA.
//				NO SANITY CHECK HERE !!!
//				THIS IS ONLY FOR INTERNAL USAGE...
//
//

void	AM79C970InitTxDescriptor(DWORD TxHead, DWORD TxBuffSize, BOOL bVirt)
{
	if (bVirt)
	{		
		/////////////////////////////////////////////////////////////////////////
		//	Need to convert the virtual addresses to physical addresses.
		//	This library runs in user mode (example: NDIS).
		//
		localDEBUGMSG ("AM79C970InitTxDescriptor::: Getting physical addresses...NOT IMPLEMENTED.\r\n");		
	}
	else
	{
		/////////////////////////////////////////////////////////////////////////
		//	Easy... Simply use the provided addresses and size to fit the 
		//	maximum number of buffer and descriptors we can...
		//	We must have been called from Bootloader or Ethdbg.
		//

		BOOL	bDone = FALSE;
		int		i	  = 1;	
		DWORD	dwCostOfOneBuffer = sizeof(TX_DESCRIPTOR_FORMAT) + MAX_BUFFER_SIZE;

		/////////////////////////////////////////////////////////////////////////
		//	Calculate the max number of descriptor + buffer that I can have 
		//	for the given size and stuff the result to global variables used by 
		//	this library..
		//

		i = TxBuffSize / dwCostOfOneBuffer;		

		dwTRANSMIT_DESCRIPTORS_HEAD = TxHead;		
		dwTRANSMIT_BUFFER_START = TxHead + i * sizeof(TX_DESCRIPTOR_FORMAT);
		dwTRANSMIT_RING_SIZE = i;

		localDEBUGMSG ("AM79C970InitTxDescriptor::: dwTRANSMIT_DESCRIPTORS_HEAD = 0x%x...\r\n", dwTRANSMIT_DESCRIPTORS_HEAD);
		localDEBUGMSG ("AM79C970InitTxDescriptor::: dwTRANSMIT_BUFFER_START     = 0x%x...\r\n", dwTRANSMIT_BUFFER_START);
		localDEBUGMSG ("AM79C970InitTxDescriptor::: dwTRANSMIT_RING_SIZE        = 0x%x...\r\n", dwTRANSMIT_RING_SIZE);
	}		

}	// AM79C970InitTxDescriptor()


/////////////////////////////////////////////////////////////////////////////////
//	AM79C970InitRxDescriptor()
//	See AM79C970InitTxDescriptor()
//

void	AM79C970InitRxDescriptor(DWORD RxHead, DWORD RxBuffSize, BOOL bVirt)
{
	if (bVirt)
	{
		/////////////////////////////////////////////////////////////////////////
		//	Need to convert the virtual addresses to physical addresses.
		//	This library runs in user mode (example: NDIS).
		//
		localDEBUGMSG ("DEC21140InitRxDescriptor::: Getting physical addresses... NOT IMPLEMENTED !!!\r\n");

	}
	else
	{
		//	Easy... Simply use the provided addresses and size to fit the maximum 
		//	number of buffer and descriptors we can...
		//	We must have been called from Bootloader or Ethdbg.

		BOOL	bDone = FALSE;
		int		i	  = 1;	
		DWORD	dwCostOfOneBuffer = sizeof(RX_DESCRIPTOR_FORMAT) + MAX_BUFFER_SIZE;

		/////////////////////////////////////////////////////////////////////////
		//	Calculate the max number of descriptor + buffer that I can have 
		//	for the given size and stuff the result to global variables used by 
		//	this library..
		//
		i = RxBuffSize / dwCostOfOneBuffer;		
		
		dwRECEIVE_DESCRIPTORS_HEAD = RxHead;					
		dwRECEIVE_BUFFER_START = RxHead + i * sizeof(RX_DESCRIPTOR_FORMAT);				
		dwRECEIVE_RING_SIZE	= i;		

		localDEBUGMSG ("AM79C970InitRxDescriptor::: dwRECEIVE_DESCRIPTORS_HEAD   = 0x%x...\r\n", dwRECEIVE_DESCRIPTORS_HEAD);
		localDEBUGMSG ("AM79C970InitRxDescriptor::: dwRECEIVE_BUFFER_START       = 0x%x...\r\n", dwRECEIVE_BUFFER_START);
		localDEBUGMSG ("AM79C970InitRxDescriptor::: dwRECEIVE_RING_SIZE          = 0x%x...\r\n", dwRECEIVE_RING_SIZE);
	}
	
}	// AM79C970InitRxDescriptor()



/////////////////////////////////////////////////////////////////////////////////
//	AM79C970QueryDescriptorSize(void) and AM79C970QueryBufferSize(void)
//
//	Caller can use these functions to figure out how many descriptors and buffer 
//	that can fit into its budget.
//	These two dimensions can change in the libary, caller should make sure that 
//	it uses these functions in determining the number of descriptor that this
//	library will use given subsequent InitTxDescriptor() and InitRxDescriptor() 
//	calls.
//
	
DWORD	AM79C970QueryDescriptorSize(void)
{
	return	sizeof(TX_DESCRIPTOR_FORMAT);

}	// AM79C970QueryRxDescriptor()


DWORD	AM79C970QueryBufferSize(void)
{
	return MAX_BUFFER_SIZE;

}	// AM79C970QueryDescriptorSize()


//------------------------------------------------------------------------------

BOOL AM79C970InitDMABuffer(UINT32 address, UINT32 size)
{
	UINT32 ulHalfBufferSize = size >> 1;

	AM79C970InitTxDescriptor(address, ulHalfBufferSize, FALSE);
	AM79C970InitRxDescriptor(address+ulHalfBufferSize, ulHalfBufferSize, FALSE);
	
	return TRUE;
}


/////////////////////////////////////////////////////////////////////////////////
//	Dump utilities...
//



/////////////////////////////////////////////////////////////////////////////////
//	Dump TX descriptors...
//
void DumpOneTxDescriptor (PTX_DESCRIPTOR_FORMAT	 pTxDesc)
{
	localDEBUGMSG ("TMD2 = 0x%x --- TMD1 = 0x%x  --- Buffer = 0x%x \r\n",
		pTxDesc->TMD2.dwReg, pTxDesc->TMD1.dwReg, pTxDesc->TBADR);

}	// DumpOneTxDescriptor()

void DumpTxDescriptors()
{
	PTX_DESCRIPTOR_FORMAT	pTxDesc;
	UINT					i;
	
	pTxDesc = (PTX_DESCRIPTOR_FORMAT) dwTRANSMIT_DESCRIPTORS_HEAD;

	for (i = 0 ; i < dwTRANSMIT_RING_SIZE ; i++)
		DumpOneTxDescriptor((PTX_DESCRIPTOR_FORMAT)(pTxDesc + i));

}	// DumpTxDescriptors()



/////////////////////////////////////////////////////////////////////////////////
//	DumpRxDescriptors...
//
void DumpOneRxDescriptor (PRX_DESCRIPTOR_FORMAT	 pRxDesc)
{
	localDEBUGMSG ("RMD2 = 0x%x --- RMD1 = 0x%x  --- Buffer = 0x%x \r\n",
		pRxDesc->RMD2.dwReg, pRxDesc->RMD1.dwReg, pRxDesc->RBADR);

}	// DumpOneTxDescriptor()


void DumpRxDescriptors()
{
	PRX_DESCRIPTOR_FORMAT	pRxDesc;
	UINT					i;
	
	pRxDesc = (PRX_DESCRIPTOR_FORMAT) dwRECEIVE_DESCRIPTORS_HEAD;

	for (i = 0 ; i < dwRECEIVE_RING_SIZE ; i++)
		DumpOneRxDescriptor((PRX_DESCRIPTOR_FORMAT)(pRxDesc + i));


}	// DumpRxDescriptors()


/////////////////////////////////////////////////////////////////////////////////
//	Other dumps...
//


void AmdDisplayHex (BYTE data)
{
	//localDEBUGMSG ("0x");
	if (data < 0x10)
		localDEBUGMSG ("0");
	localDEBUGMSG ("%x ", data);
}	// DisplayHex()


static void DumpMemory (PBYTE	pSource, DWORD	dwLength)
{
	int		i = 0;

	localDEBUGMSG ("+---- MEM DUMP (%d bytes)----+\r\n", dwLength);
	localDEBUGMSG ("0x%x: ", pSource);
	while (dwLength--)
	{	
		AmdDisplayHex (*pSource++);
		if (++i == 16)
		{
			i = 0;		
			localDEBUGMSG ("\r\n0x%x: ", pSource);
		}		
	}
	localDEBUGMSG ("\r\n\r\n");
}	// DumpMemory()


void PrintAddr (BYTE bData)
{
	if (bData < 10)	
		localDEBUGMSG ("0");
	localDEBUGMSG ("%x", bData);
}

void DumpSenderAddr (PBYTE pData)
{	
	PrintAddr (*pData++);
	PrintAddr (*pData++);
	PrintAddr (*pData++);
	PrintAddr (*pData++);
	PrintAddr (*pData++);
	PrintAddr (*pData++);	
	localDEBUGMSG ("\r\n");
}






