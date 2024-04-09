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

	send.c

Abstract:

	This file contains the code for transmitting a packet.

Environment:

	Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:

	$Log:   V:/pvcs/archives/network/pcnet/homelan/wince/mini4/src/send.c_y  $
 * 
 *    Rev 1.0   Apr 07 1999 18:13:42   jagannak
 * Initial check-in
 * 
 *    Rev 1.9   Mar 31 1999 12:08:30   cabansag
 *  
 * 
 *    Rev 1.53   20 Aug 1997 14:23:54   steiger
 * Further refinements to multi-send and multi-receive.
 * 
 * 
 *    Rev 1.52   12 Aug 1997 17:51:08   steiger
 * Multi-RX implemented. Copy/Compare testing has been done.

--*/


#include <ndis.h>
#include <efilter.h>
#include "lancehrd.h"
#include "lancesft.h"


#ifdef NDIS40_PLUS

#if DBG
	UINT PeakPkts = 0;
#endif

STATIC
VOID
EnableTxInts (
	IN PLANCE_ADAPTER Adapter
	);

STATIC
VOID
TxReset (
	IN PLANCE_ADAPTER Adapter
	);

VOID
LanceSendPackets(
	IN NDIS_HANDLE MiniportAdapterContext,
	IN PPNDIS_PACKET PacketArray,
	IN UINT NumberOfPackets
	)

/*++

Routine Description:

	LanceSendPackets () satisfies the requirements for MiniportSendPackets ()
	as defined in the NDIS specification.

Arguments:

	MiniportAdapterContext -
		The context registered with the wrapper, really a pointer to the
		adapter structure.

	PacketArray -
		A pointer to an array of descriptors for packet(s) that is(are) to be
		transmitted.

	NumberOfPackets -
		Number of valid packet descriptors in PacketArray.

Return Value:

	None.

--*/

{
//	PLANCE_TRANSMIT_DESCRIPTOR		CurrentDescriptor;
	PLANCE_TRANSMIT_DESCRIPTOR_HI	CurrentDescriptorHi;
	PLANCE_ADAPTER					Adapter = (PLANCE_ADAPTER)MiniportAdapterContext;
	UINT							TotalPacketSize;
	PCHAR							CurrentDestination;
	INT								TotalDataMoved = 0;
	USHORT							Csr0Value;
	USHORT							CurrentDescriptorIndex;
	ULONG							TransmitStatus;
	USHORT 						TransmitError;
	CHAR							Destination[ETH_LENGTH_OF_ADDRESS];
	UINT							AddressLength;
	NDIS_BUFFER						Buffer;
	PNDIS_PACKET_OOB_DATA			OobData;
	BOOLEAN						IsPktCopied = FALSE;

	#if DBG
	if (LanceDbg)
		DbgPrint("==>LanceSendPackets Qty.:%d\n", NumberOfPackets);
	if (NumberOfPackets > PeakPkts)
	{
		PeakPkts = NumberOfPackets;
	}
	#endif

// for HomeLan
	if (Adapter->ActivePhyAddress == HOMERUN_PHY_ADDRESS)
	{
		LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, &Csr0Value);
		
		if ((Csr0Value & LANCE_CSR0_RUNNING) != LANCE_CSR0_RUNNING)
		{
			#if DBG
			if (LanceInitDbg)
			{
				DbgPrint("LanceHandleInterrupt: Chip stopped. CSR0 = %x\n", Csr0Value);
				DbgPrint("LanceSendPackets calls LanceInit:Chip Running.\n");
			}
			#endif
	
			LanceInit(Adapter,FALSE);
		}
	}

#ifdef NDIS40_MINIPORT
	if (Adapter->ActivePhyAddress == HOMERUN_PHY_ADDRESS)
	{
#endif
	NdisAcquireSpinLock(&Adapter->TxQueueLock);
	if(Adapter->NumPacketsQueued)
	{
		while (NumberOfPackets--) {
				Adapter->NumPacketsQueued++;
				EnqueuePacket(Adapter->FirstTxQueue, Adapter->LastTxQueue, *PacketArray);

				NDIS_SET_PACKET_STATUS(*PacketArray,NDIS_STATUS_PENDING);
				PacketArray++;
		}
		NumberOfPackets = 0;
	}
	NdisReleaseSpinLock(&Adapter->TxQueueLock);

#ifdef NDIS40_MINIPORT
	}
#endif

	#if DBG
		if (LanceDbg)
		DbgPrint ("Enter Double Copy\n");
	#endif

	while (NumberOfPackets--)
	{
		if(Adapter->OpFlags & LANCE_RESET_CALLED)
			return;

#ifdef NDIS50_PLUS
		/* If PM mode is on, simply return */
		if (Adapter->OpFlags & PM_MODE)
		{
			while (NumberOfPackets--) {
				NDIS_SET_PACKET_STATUS(*PacketArray,NDIS_STATUS_SUCCESS);
				PacketArray++;
			}
		#ifdef DBG
			if (LancePMDbg)
			DbgPrint("LanceSendPackets : PM MODE is On\n");
		#endif
			return;
		}
#endif

		/* Set no-reset flag for ISR routine */
//		Adapter->OpFlags |= RESET_PROHIBITED;

		/* Get a pointer to the out of band data for this packet */
		OobData = NDIS_OOB_DATA_FROM_PACKET (*PacketArray);

		/* Get the current xmit descriptor. */
		CurrentDescriptorIndex	= (USHORT)Adapter->NextTransmitDescriptorIndex;

		#if DBG
		if (LanceDbg || LanceSendDbg)
		{
//			DbgPrint("Descriptor:%i,%i\n",CurrentDescriptorIndex,Adapter->NextTransmitDescriptorIndex);
//				DbgBreakPoint();
		}	
		#endif

		CurrentDescriptorHi	= (PLANCE_TRANSMIT_DESCRIPTOR_HI)Adapter->TransmitDescriptorRing + CurrentDescriptorIndex;
		TransmitStatus		= (ULONG)CurrentDescriptorHi->LanceTMDFlags;
		TransmitError =	CurrentDescriptorHi->TransmitError;

		if (TransmitStatus & OWN)
		{
			#if DBG
			if (LanceDbg || LanceSendDbg)
			{
				if(TransmitStatus != (ULONG)CurrentDescriptorHi->LanceTMDFlags)
				DbgPrint("No Match!!!\n");
			}	
			#endif

			#if DBG
			if (LanceDbg || LanceSendDbg)
			{
				DbgPrint("no xmit descriptor available.\n");
			}	
			#endif

//			Adapter->OpFlags &= ~RESET_PROHIBITED;

			NumberOfPackets++;
			if(IsPktCopied)
			{
				LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, &Csr0Value);
				Csr0Value &= LANCE_CSR0_IENA;
				LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, Csr0Value | LANCE_CSR0_TDMD);
				IsPktCopied = FALSE;
			}

			/* TxQueue */
#ifdef NDIS40_MINIPORT
	if (Adapter->ActivePhyAddress == HOMERUN_PHY_ADDRESS)
	{
#endif
			NdisAcquireSpinLock(&Adapter->TxQueueLock);
			while (NumberOfPackets--) {
            Adapter->NumPacketsQueued++;
            EnqueuePacket(Adapter->FirstTxQueue, Adapter->LastTxQueue, *PacketArray);

				NDIS_SET_PACKET_STATUS(*PacketArray,NDIS_STATUS_PENDING);
				PacketArray++;
			}
			NdisReleaseSpinLock(&Adapter->TxQueueLock);

			NdisMSetTimer(&(Adapter->DeferredProcessTimer),1);
#ifdef NDIS40_MINIPORT
	}
	else
	{
			while (NumberOfPackets--) {
				NDIS_SET_PACKET_STATUS(*PacketArray,NDIS_STATUS_RESOURCES);
				PacketArray++;
			}
	}
#endif
			Adapter->OpFlags |= TX_RESOURCES;
			break;
		}
		
		//
		// At least one packet will be copied into the TransmitBuffer.
		//
		IsPktCopied = TRUE;

		/* Send packet on the wire */
		/* Get the current xmit data buffer address */

		CurrentDestination = Adapter->TransmitBufferPointer +
						(CurrentDescriptorIndex * TRANSMIT_BUFFER_SIZE);

	//
	// As we do not update the statistics in the ISR, we need to
	// update them here before we use this descriptor again.
	// Check if the packet completed OK, and update statistics.
	//
	if (TransmitStatus & (DERR | LANCE_TRANSMIT_DEF_ERROR))
	{
		//
		// Update general error statistics
		//
		++Adapter->GeneralMandatory[GM_TRANSMIT_BAD];

#ifdef DMI
		if (TransmitStatus & LANCE_TRANSMIT_BUF_ERROR)
			++Adapter->DmiSpecific[DMI_TXBUFF_ERR];

		if (TransmitStatus & LANCE_TRANSMIT_EXDEF_ERROR)
			++Adapter->DmiSpecific[DMI_EX_DEFER];
#endif //DMI

		if (TransmitStatus & LANCE_TRANSMIT_DEF_ERROR )
		++Adapter->MediaOptional[MO_TRANSMIT_DEFERRED];

		if (TransmitStatus & DERR)
		{
			if (TransmitError & LANCE_TRANSMIT_UFLO_ERROR)
			{
				++Adapter->MediaOptional[MO_TRANSMIT_UNDERRUN];
				#if DBG
				if (LanceDbg)
					DbgPrint("Transmit: UFLO error detected\n");
				#endif
			}

			if (TransmitError & LANCE_TRANSMIT_LCAR_ERROR)
			{
				if (Adapter->ActivePhyAddress != HOMERUN_PHY_ADDRESS)
					++Adapter->MediaOptional[MO_TRANSMIT_TIMES_CRS_LOST];
			}
		
			if (TransmitError & LANCE_TRANSMIT_LCOL_ERROR)
				++Adapter->MediaOptional[MO_TRANSMIT_LATE_COLLISIONS];
			if (TransmitError & LANCE_TRANSMIT_RTRY_ERROR)
				++Adapter->MediaOptional[MO_TRANSMIT_MAX_COLLISIONS];
			if (TransmitError & LANCE_TRANSMIT_BUF_ERROR)
			{
				#if DBG
				if (LanceDbg)
					DbgPrint("Transmit: BUF error detected\n");
				#endif
			}
		}
	}
	else
	{
		Adapter->LinkActive = TRUE;

		// Update general good statistics
		//
		++Adapter->GeneralMandatory[GM_TRANSMIT_GOOD];

		if (TransmitStatus & LANCE_TRANSMIT_ONE_COLLISION)
			++Adapter->MediaMandatory[MM_TRANSMIT_ONE_COLLISION];
		if	(TransmitStatus & LANCE_TRANSMIT_MORE_COLLISION)
			++Adapter->MediaMandatory[MM_TRANSMIT_MORE_COLLISIONS];

		//
		// Get saved packet information for this descriptor
		//

#ifdef DMI
		Adapter->DmiSpecific[DMI_TX_BYTES] += (COUNTER64) Adapter->TransmitPacketLength[CurrentDescriptorIndex];
#endif //DMI

		switch (Adapter->TransmitPacketType[CurrentDescriptorIndex])
		{
			case LANCE_DIRECTED:
				++Adapter->GeneralOptionalFrameCount[GO_DIRECTED_TRANSMITS];
//				LanceAddUlongToLargeInteger(
//					&Adapter->GeneralOptionalByteCount[GO_DIRECTED_TRANSMITS],
//					Adapter->TransmitPacketLength[CurrentDescriptorIndex]);
				break;

			case LANCE_MULTICAST:
				++Adapter->GeneralOptionalFrameCount[GO_MULTICAST_TRANSMITS];
//				LanceAddUlongToLargeInteger(
//					&Adapter->GeneralOptionalByteCount[GO_MULTICAST_TRANSMITS],
//					Adapter->TransmitPacketLength[CurrentDescriptorIndex]);
				break;

			case LANCE_BROADCAST:
				++Adapter->GeneralOptionalFrameCount[GO_BROADCAST_TRANSMITS];
		}
	}

		/* Save packet information. */
		/* Copy address to buffer. */

		LanceMovePacket(Adapter,
			*PacketArray,
			ETH_LENGTH_OF_ADDRESS,
			Destination,
			&AddressLength,
			TRUE
			);

		ASSERT(AddressLength == ETH_LENGTH_OF_ADDRESS);

		/* Get packet flavor */

		if (ETH_IS_MULTICAST(Destination))
		{
			Adapter->TransmitPacketType[CurrentDescriptorIndex] = 
			(ETH_IS_BROADCAST(Destination)) ? LANCE_BROADCAST : LANCE_MULTICAST;

		}
		else
		{
			Adapter->TransmitPacketType[CurrentDescriptorIndex] = LANCE_DIRECTED;
		}

		/* Now fill in the adapter buffer with the data	*/
		/* from the users buffers. */

		NdisQueryPacket(
			*PacketArray,
			NULL,
			NULL,
			NULL,
			&TotalPacketSize
			);

		/* Save packet length information */

		Adapter->TransmitPacketLength[CurrentDescriptorIndex] = TotalPacketSize;

		LanceMovePacket(Adapter,
					*PacketArray,
					TotalPacketSize,
					CurrentDestination,
					&TotalDataMoved,
					TRUE
					);

#if 1
		Buffer.Next = NULL;
//		Buffer.Size = 0;
		Buffer.MdlFlags = 0;
//		Buffer.Process = 0;
		Buffer.MappedSystemVa = CurrentDestination;
		Buffer.StartVa = CurrentDestination;
		Buffer.ByteCount = TotalPacketSize;
		Buffer.ByteOffset = 0;
#else
		// jk following 3 lines - NDIS_BUFFER struct conforms to the Ndis.h 
		// file as defined in Windows CE 
		Buffer.Next=NULL;
		Buffer.VirtualAddress = CurrentDestination;
		Buffer.BufferLength= TotalPacketSize;
#endif

		NdisFlushBuffer (&Buffer, TRUE);

		NdisMUpdateSharedMemory (Adapter->LanceMiniportHandle,
								TotalPacketSize,
								CurrentDestination,
								Adapter->TransmitBufferPointerPhysical +
								(CurrentDescriptorIndex	* TRANSMIT_BUFFER_SIZE)
								);

		CurrentDescriptorHi->LanceBufferPhysicalLow =
		LANCE_GET_LOW_PART_ADDRESS(
		NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
		(CurrentDescriptorIndex	* TRANSMIT_BUFFER_SIZE));

		CurrentDescriptorHi->LanceBufferPhysicalHighL =
		LANCE_GET_HIGH_PART_ADDRESS(
		NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
		(CurrentDescriptorIndex	* TRANSMIT_BUFFER_SIZE));

		CurrentDescriptorHi->LanceBufferPhysicalHighH =
		LANCE_GET_HIGH_PART_ADDRESS_H(
		NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
		(CurrentDescriptorIndex	* TRANSMIT_BUFFER_SIZE));

		if (TotalDataMoved < LANCE_MIN_PACKET_SIZE)
		{
			CurrentDescriptorHi->ByteCount = -LANCE_MIN_PACKET_SIZE;
		}
		else
		{
			CurrentDescriptorHi->ByteCount = -TotalDataMoved;
		}

		/* Now change the ownership of the packet to Lance. */
		/* STP and ENP bits are permanently set as all packets should */
		/* fit in one buffer only. Also clear the error bits. */

		CurrentDescriptorHi->LanceTMDFlags &=
			~(LANCE_TRANSMIT_MORE_COLLISION |
			LANCE_TRANSMIT_ONE_COLLISION |
			LANCE_TRANSMIT_DEF_ERROR |
			DERR);
		
		CurrentDescriptorHi->TransmitError = 0;
		
		OobData->Status = NDIS_STATUS_SUCCESS;
		/* Set STP and ENP bits. */

		CurrentDescriptorHi->LanceTMDFlags |= (STP | ENP);

		/* Set OWN bit so chip can start transfer */

		CurrentDescriptorHi->LanceTMDFlags |= OWN;

		/* Increment the next available xmit descriptor index. */
		PacketArray++;
		if (++Adapter->NextTransmitDescriptorIndex >= TRANSMIT_BUFFERS)
		{
			Adapter->NextTransmitDescriptorIndex = 0;
		}

		#if DBG
		if (LanceDbg)
			DbgPrint("LanceSendPackets: Packet Transmitted.\n");
		#endif
	} //while

	if (IsPktCopied)
	{
		LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, &Csr0Value);
		Csr0Value &= LANCE_CSR0_IENA;
		LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, Csr0Value | LANCE_CSR0_TDMD);
	}

/* TxQueue */
#ifdef NDIS40_MINIPORT
	if (Adapter->ActivePhyAddress == HOMERUN_PHY_ADDRESS)
#endif
	if(!(Adapter->OpFlags & LANCE_DEFERRED_SEND_PROCESSING))
		LanceDeferredSend(Adapter);

	#if DBG
		if (LanceDbg)
		DbgPrint("<==LanceSendPackets\n");
	#endif
}	
STATIC
VOID
TxReset (
	IN PLANCE_ADAPTER Adapter
	)
{
	#if DBG
	if (LanceDbg)
	{
		DbgPrint("TXError:reset\n");
	}
	#endif

	if(!(Adapter->OpFlags & IN_INTERRUPT_DPC))
	{
		#ifdef DBG
			if (LanceInitDbg)
			{
				DbgPrint("LanceInit is called due in TxReset\n");
			}
		#endif
		LanceInit(Adapter, FALSE);
	}
	else	/* This should never happen */
	{
		#if DBG
		if (LanceDbg)
		{
			DbgPrint("Unable to reset - IN_INTERRUPT_DPC\n");
		}
		#endif
	}
}

#else	/* start *NOT* NDIS40_PLUS */

NDIS_STATUS
LanceSend(
	IN NDIS_HANDLE MiniportAdapterContext,
	IN PNDIS_PACKET Packet,
	IN UINT					Flags
	)

/*++

Routine Description:

	The LanceSend request instructs the driver to transmit a packet through
	the adapter onto the medium.	The wrapper send its request only when
	the driver is free.	So this routine can be interrupted but can not be
	retried.	No request queuing mechanism is implemented.

Arguments:

	MiniportAdapterContext - The context registered with the wrapper,
							really a pointer to the adapter

	Packet - A pointer to a descriptor for the packet that is to be
			transmitted.

	SendFlags - Optional send flags

Return Value:

	The function value is the status of the operation.

--*/

{

//	PLANCE_TRANSMIT_DESCRIPTOR CurrentDescriptor;
	PLANCE_TRANSMIT_DESCRIPTOR_HI CurrentDescriptorHi;

	//
	// Holds the status that should be returned to the caller.
	//
	PLANCE_ADAPTER Adapter = (PLANCE_ADAPTER)MiniportAdapterContext;
	USHORT Csr0Value;
	UINT TotalPacketSize;

	//
	// Index to next descriptor available.
	//
	USHORT CurrentDescriptorIndex;

	//
	// Will point into the virtual address space addressed
	// by the adpter buffer.
	//
	PCHAR CurrentDestination;

	//
	// Transmit status of the packet.
	//
	USHORT TransmitStatus;

	//
	// Transmit error of the packet.
	//
	USHORT TransmitError;

	//
	// Will hold the total amount of data copied to the
	// adapter buffer.
	//
	INT TotalDataMoved = 0;

	CHAR Destination[ETH_LENGTH_OF_ADDRESS];
	UINT AddressLength;
	NDIS_BUFFER	Buffer;

	#if DBG
		if (LanceDbg)
		DbgPrint("==>LanceSend\n");
	#endif

	#if DBG
		if (LanceDbg)
		DbgPrint ("Enter Double Copy\n");
	#endif

	//
	// Set no-reset flag for ISR routine
	//
//	Adapter->OpFlags |= RESET_PROHIBITED;

	//
	// Check if there is a descriptor available for this request
	//
	//
	// Get the current xmit descriptor.
	//
	CurrentDescriptorIndex = Adapter->NextTransmitDescriptorIndex;
	CurrentDescriptorHi = (PLANCE_TRANSMIT_DESCRIPTOR_HI)Adapter->TransmitDescriptorRing + CurrentDescriptorIndex;
	TransmitStatus =	CurrentDescriptorHi->LanceTMDFlags;
	TransmitError =	CurrentDescriptorHi->TransmitError;
	if (TransmitStatus & OWN)
	{
		#if DBG
		if (LanceDbg)
			DbgPrint("LanceSend routine: no xmit descriptor available.\n");
		#endif

//			Adapter->OpFlags &= ~RESET_PROHIBITED;
		return NDIS_STATUS_RESOURCES;
	}

	///////////////////////////////////
	// Send packet on the wire
	///////////////////////////////////

	//
	// Get the current xmit data buffer address
	//
	CurrentDestination = Adapter->TransmitBufferPointer +
					(CurrentDescriptorIndex * TRANSMIT_BUFFER_SIZE);

	//
	// As we do not update the statistics in the ISR, we need to
	// update them here before we use this descriptor again.
	// Check if the packet completed OK, and update statistics.
	//
	if (TransmitStatus & (DERR | LANCE_TRANSMIT_DEF_ERROR))
	{
		//
		// Update general error statistics
		//
		++Adapter->GeneralMandatory[GM_TRANSMIT_BAD];

		if (TransmitStatus & LANCE_TRANSMIT_DEF_ERROR )
		++Adapter->MediaOptional[MO_TRANSMIT_DEFERRED];

		if (TransmitStatus & DERR)
		{
			if (TransmitError & LANCE_TRANSMIT_UFLO_ERROR)
			{
				++Adapter->MediaOptional[MO_TRANSMIT_UNDERRUN];
				#if DBG
				if (LanceDbg)
					DbgPrint("Transmit: UFLO error detected\n");
				#endif
			}

			if (TransmitError & LANCE_TRANSMIT_LCAR_ERROR)
			{
				if (Adapter->ActivePhyAddress != HOMERUN_PHY_ADDRESS)
					++Adapter->MediaOptional[MO_TRANSMIT_TIMES_CRS_LOST];
			}
		
			if (TransmitError & LANCE_TRANSMIT_LCOL_ERROR)
				++Adapter->MediaOptional[MO_TRANSMIT_LATE_COLLISIONS];
			if (TransmitError & LANCE_TRANSMIT_RTRY_ERROR)
				++Adapter->MediaOptional[MO_TRANSMIT_MAX_COLLISIONS];
			if (TransmitError & LANCE_TRANSMIT_BUF_ERROR)
			{
				#if DBG
				if (LanceDbg)
					DbgPrint("Transmit: BUF error detected\n");
				#endif
			}

		}
	}
	else
	{
		Adapter->LinkActive = TRUE;

		//
		// Update general good statistics
		//
		++Adapter->GeneralMandatory[GM_TRANSMIT_GOOD];

		if (TransmitStatus & LANCE_TRANSMIT_ONE_COLLISION)
			++Adapter->MediaMandatory[MM_TRANSMIT_ONE_COLLISION];
		if	(TransmitStatus & LANCE_TRANSMIT_MORE_COLLISION)
			++Adapter->MediaMandatory[MM_TRANSMIT_MORE_COLLISIONS];

		//
		// Get saved packet information for this descriptor
		//
		switch (Adapter->TransmitPacketType[CurrentDescriptorIndex])
		{
			case LANCE_DIRECTED:
				++Adapter->GeneralOptionalFrameCount[GO_DIRECTED_TRANSMITS];
//				LanceAddUlongToLargeInteger(
//					&Adapter->GeneralOptionalByteCount[GO_DIRECTED_TRANSMITS],
//					Adapter->TransmitPacketLength[CurrentDescriptorIndex]);
				break;

			case LANCE_MULTICAST:
				++Adapter->GeneralOptionalFrameCount[GO_MULTICAST_TRANSMITS];
//				LanceAddUlongToLargeInteger(
//					&Adapter->GeneralOptionalByteCount[GO_MULTICAST_TRANSMITS],
//					Adapter->TransmitPacketLength[CurrentDescriptorIndex]);
				break;

			case LANCE_BROADCAST:
				++Adapter->GeneralOptionalFrameCount[GO_BROADCAST_TRANSMITS];
//				LanceAddUlongToLargeInteger(
//					&Adapter->GeneralOptionalByteCount[GO_BROADCAST_TRANSMITS],
//					Adapter->TransmitPacketLength[CurrentDescriptorIndex]);
		}
	}
	//
	// Save sending packet information.
	// Copy address to buffer.
	//
	LanceMovePacket(Adapter,
		Packet,
		ETH_LENGTH_OF_ADDRESS,
		Destination,
		&AddressLength,
		TRUE
		);

	ASSERT(AddressLength == ETH_LENGTH_OF_ADDRESS);

	//
	// Save packet type
	//
	if (ETH_IS_MULTICAST(Destination))
	{
		Adapter->TransmitPacketType[CurrentDescriptorIndex] = 
		(ETH_IS_BROADCAST(Destination)) ? LANCE_BROADCAST : LANCE_MULTICAST;
	}
	else
	{
		Adapter->TransmitPacketType[CurrentDescriptorIndex] = LANCE_DIRECTED;
	}

	//
	// Now fill in the adapter buffer with the data 
	// from the users buffers.
	//
	NdisQueryPacket(
		Packet,
		NULL,
		NULL,
		NULL,
		&TotalPacketSize
		);

	//
	// Save packet length information
	//
	Adapter->TransmitPacketLength[CurrentDescriptorIndex] = TotalPacketSize;

	LanceMovePacket(Adapter,
				Packet,
				TotalPacketSize,
				CurrentDestination,
				&TotalDataMoved,
				TRUE
				);

#if 1
	Buffer.Next = NULL;
	Buffer.MdlFlags = 0;
	Buffer.MappedSystemVa = CurrentDestination;
	Buffer.StartVa = CurrentDestination;
	Buffer.ByteCount = TotalPacketSize;
	Buffer.ByteOffset = 0;
#else
	// jk following 3 lines - NDIS_BUFFER struct conforms to the Ndis.h 
	// file as defined in Windows CE 
	Buffer.Next=NULL;
	Buffer.VirtualAddress = CurrentDestination;
	Buffer.BufferLength= TotalPacketSize;
#endif

	NdisFlushBuffer (&Buffer, TRUE);

	NdisMUpdateSharedMemory (Adapter->LanceMiniportHandle,
							TotalPacketSize,
							CurrentDestination,
							Adapter->TransmitBufferPointerPhysical +
							(CurrentDescriptorIndex	* TRANSMIT_BUFFER_SIZE)
							);
							
	CurrentDescriptorHi->LanceBufferPhysicalLow =
	LANCE_GET_LOW_PART_ADDRESS(
		NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
		(CurrentDescriptorIndex	* TRANSMIT_BUFFER_SIZE));

	CurrentDescriptorHi->LanceBufferPhysicalHighL =
	LANCE_GET_HIGH_PART_ADDRESS(
		NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
		(CurrentDescriptorIndex	* TRANSMIT_BUFFER_SIZE));

	CurrentDescriptorHi->LanceBufferPhysicalHighH =
	LANCE_GET_HIGH_PART_ADDRESS_H(
		NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
		(CurrentDescriptorIndex	* TRANSMIT_BUFFER_SIZE));

	if (TotalDataMoved < LANCE_MIN_PACKET_SIZE)
	{
		CurrentDescriptorHi->ByteCount = -LANCE_MIN_PACKET_SIZE;
	}
	else
	{
		CurrentDescriptorHi->ByteCount = -TotalDataMoved;
	}
	//
	// Now change the ownership of the packet to Lance.
	// STP and ENP bits are permanently set as all packets should
	// fit in one buffer only. Also clear the error bits.
	//
	CurrentDescriptorHi->LanceTMDFlags &=
		~(LANCE_TRANSMIT_MORE_COLLISION |
		LANCE_TRANSMIT_ONE_COLLISION |
		LANCE_TRANSMIT_DEF_ERROR |
		DERR);
	
	CurrentDescriptorHi->TransmitError = 0;
	
	//
	// Set STP and ENP bits.
	//
	CurrentDescriptorHi->LanceTMDFlags |= (STP | ENP);

	// 
	// Toggle OWN bit so chip can start transfer
	// 
	CurrentDescriptorHi->LanceTMDFlags |= OWN;

	// 
	// Start chip now to send packet on the wire
	//
	LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, &Csr0Value);
	Csr0Value &= LANCE_CSR0_IENA;
	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, Csr0Value | LANCE_CSR0_TDMD);

	//
	// Increment the next available xit descriptor index.
	//
	if (++Adapter->NextTransmitDescriptorIndex >= TRANSMIT_BUFFERS)
	{
		Adapter->NextTransmitDescriptorIndex = 0;
	}

//	Adapter->OpFlags &= ~RESET_PROHIBITED;

	#if DBG
		if (LanceDbg)
		DbgPrint("LanceSendPacket: packet transmitted.\n");
	#endif

	//
	// Return good status
	//

	#if DBG
	if (LanceDbg)
		DbgPrint("<==LanceSend\n");
	#endif
	return NDIS_STATUS_SUCCESS;
}	
#endif	/* end *NOT* NDIS40_PLUS */

STATIC
VOID
EnableTxInts (
	IN PLANCE_ADAPTER Adapter
	)
{
	USHORT		Data;

 	LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR3, &Data);
 	Data &= ~LANCE_CSR3_TINTM;
 	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR3, Data);
}

/* TxQueue */
STATIC
VOID
SendQueuedPacket(
	IN PLANCE_ADAPTER Adapter,
	IN PNDIS_PACKET Packet,
	IN USHORT CurrentDescriptorIndex
	)
{
//	PLANCE_TRANSMIT_DESCRIPTOR CurrentDescriptor;
	PLANCE_TRANSMIT_DESCRIPTOR_HI CurrentDescriptorHi;

	//
	// Holds the status that should be returned to the caller.
	//
	USHORT Csr0Value;
	UINT TotalPacketSize;

	//
	// Will point into the virtual address space addressed
	// by the adpter buffer.
	//
	PCHAR CurrentDestination;

	//
	// Transmit status of the packet.
	//
	USHORT TransmitStatus;

	//
	// Transmit error of the packet.
	//
	USHORT TransmitError;

	//
	// Will hold the total amount of data copied to the
	// adapter buffer.
	//
	INT TotalDataMoved = 0;

	CHAR Destination[ETH_LENGTH_OF_ADDRESS];
	UINT AddressLength;
	NDIS_BUFFER	Buffer;

	#if DBG
		if (LanceDbg)
		DbgPrint("==>SendQueuedPacket\n");
	#endif

	#if DBG
		if (LanceDbg)
		DbgPrint ("Enter Double Copy\n");
	#endif

	//
	// Set no-reset flag for ISR routine
	//
//	Adapter->OpFlags |= RESET_PROHIBITED;

	//
	// Check if there is a descriptor available for this request
	//
	CurrentDescriptorHi = (PLANCE_TRANSMIT_DESCRIPTOR_HI)Adapter->TransmitDescriptorRing + CurrentDescriptorIndex;
	TransmitStatus =	CurrentDescriptorHi->LanceTMDFlags;
	TransmitError =	CurrentDescriptorHi->TransmitError;

	if (TransmitStatus & OWN)
	{
		#if DBG
		if (LanceDbg)
			DbgPrint("BugBug!!! No Descriptor.\n");
		#endif

//		Adapter->OpFlags &= ~RESET_PROHIBITED;
		NdisMSendComplete(
						Adapter->LanceMiniportHandle,
						Packet,
						NDIS_STATUS_FAILURE);
		return;
	}

	///////////////////////////////////
	// Send packet on the wire
	///////////////////////////////////

	//
	// Get the current xmit data buffer address
	//
	CurrentDestination = Adapter->TransmitBufferPointer +
					(CurrentDescriptorIndex * TRANSMIT_BUFFER_SIZE);

	//
	// As we do not update the statistics in the ISR, we need to
	// update them here before we use this descriptor again.
	// Check if the packet completed OK, and update statistics.
	//
	if (TransmitStatus & (DERR | LANCE_TRANSMIT_DEF_ERROR))
	{
		//
		// Update general error statistics
		//
		++Adapter->GeneralMandatory[GM_TRANSMIT_BAD];

		if (TransmitStatus & LANCE_TRANSMIT_DEF_ERROR )
		++Adapter->MediaOptional[MO_TRANSMIT_DEFERRED];

		if (TransmitStatus & DERR)
		{
			if (TransmitError & LANCE_TRANSMIT_UFLO_ERROR)
			{
				++Adapter->MediaOptional[MO_TRANSMIT_UNDERRUN];
				#if DBG
				if (LanceDbg)
					DbgPrint("Transmit: UFLO error detected\n");
				#endif
			}

			if (TransmitError & LANCE_TRANSMIT_LCAR_ERROR)
			{
				if (Adapter->ActivePhyAddress != HOMERUN_PHY_ADDRESS)
					++Adapter->MediaOptional[MO_TRANSMIT_TIMES_CRS_LOST];
			}
		
			if (TransmitError & LANCE_TRANSMIT_LCOL_ERROR)
				++Adapter->MediaOptional[MO_TRANSMIT_LATE_COLLISIONS];
			if (TransmitError & LANCE_TRANSMIT_RTRY_ERROR)
				++Adapter->MediaOptional[MO_TRANSMIT_MAX_COLLISIONS];
			if (TransmitError & LANCE_TRANSMIT_BUF_ERROR)
			{
				#if DBG
				if (LanceDbg)
					DbgPrint("Transmit: BUF error detected\n");
				#endif
			}

		}
	}
	else
	{
		Adapter->LinkActive = TRUE;

		//
		// Update general good statistics
		//
		++Adapter->GeneralMandatory[GM_TRANSMIT_GOOD];

		if (TransmitStatus & LANCE_TRANSMIT_ONE_COLLISION)
			++Adapter->MediaMandatory[MM_TRANSMIT_ONE_COLLISION];
		if	(TransmitStatus & LANCE_TRANSMIT_MORE_COLLISION)
			++Adapter->MediaMandatory[MM_TRANSMIT_MORE_COLLISIONS];

		//
		// Get saved packet information for this descriptor
		//
		switch (Adapter->TransmitPacketType[CurrentDescriptorIndex])
		{
			case LANCE_DIRECTED:
				++Adapter->GeneralOptionalFrameCount[GO_DIRECTED_TRANSMITS];
				break;

			case LANCE_MULTICAST:
				++Adapter->GeneralOptionalFrameCount[GO_MULTICAST_TRANSMITS];
				break;

			case LANCE_BROADCAST:
				++Adapter->GeneralOptionalFrameCount[GO_BROADCAST_TRANSMITS];
		}
	}
	//
	// Save sending packet information.
	// Copy address to buffer.
	//
	LanceMovePacket(Adapter,
		Packet,
		ETH_LENGTH_OF_ADDRESS,
		Destination,
		&AddressLength,
		TRUE
		);

	ASSERT(AddressLength == ETH_LENGTH_OF_ADDRESS);

	//
	// Save packet type
	//
	if (ETH_IS_MULTICAST(Destination))
	{
		Adapter->TransmitPacketType[CurrentDescriptorIndex] = 
		(ETH_IS_BROADCAST(Destination)) ? LANCE_BROADCAST : LANCE_MULTICAST;
	}
	else
	{
		Adapter->TransmitPacketType[CurrentDescriptorIndex] = LANCE_DIRECTED;
	}

	//
	// Now fill in the adapter buffer with the data 
	// from the users buffers.
	//
	NdisQueryPacket(
		Packet,
		NULL,
		NULL,
		NULL,
		&TotalPacketSize
		);

	//
	// Save packet length information
	//
	Adapter->TransmitPacketLength[CurrentDescriptorIndex] = TotalPacketSize;

	LanceMovePacket(Adapter,
				Packet,
				TotalPacketSize,
				CurrentDestination,
				&TotalDataMoved,
				TRUE
				);

#if 1
	Buffer.Next = NULL;
	Buffer.MdlFlags = 0;
	Buffer.MappedSystemVa = CurrentDestination;
	Buffer.StartVa = CurrentDestination;
	Buffer.ByteCount = TotalPacketSize;
	Buffer.ByteOffset = 0;
#else
	// jk following 3 lines - NDIS_BUFFER struct conforms to the Ndis.h 
	// file as defined in Windows CE 
	Buffer.Next=NULL;
	Buffer.VirtualAddress = CurrentDestination;
	Buffer.BufferLength= TotalPacketSize;
#endif

	NdisFlushBuffer (&Buffer, TRUE);

	NdisMUpdateSharedMemory (Adapter->LanceMiniportHandle,
							TotalPacketSize,
							CurrentDestination,
							Adapter->TransmitBufferPointerPhysical +
							(CurrentDescriptorIndex	* TRANSMIT_BUFFER_SIZE)
							);
							
	CurrentDescriptorHi->LanceBufferPhysicalLow =
	LANCE_GET_LOW_PART_ADDRESS(
		NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
		(CurrentDescriptorIndex	* TRANSMIT_BUFFER_SIZE));

	CurrentDescriptorHi->LanceBufferPhysicalHighL =
	LANCE_GET_HIGH_PART_ADDRESS(
		NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
		(CurrentDescriptorIndex	* TRANSMIT_BUFFER_SIZE));

	CurrentDescriptorHi->LanceBufferPhysicalHighH =
	LANCE_GET_HIGH_PART_ADDRESS_H(
		NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
		(CurrentDescriptorIndex	* TRANSMIT_BUFFER_SIZE));

	if (TotalDataMoved < LANCE_MIN_PACKET_SIZE)
	{
		CurrentDescriptorHi->ByteCount = -LANCE_MIN_PACKET_SIZE;
	}
	else
	{
		CurrentDescriptorHi->ByteCount = -TotalDataMoved;
	}
	//
	// Now change the ownership of the packet to Lance.
	// STP and ENP bits are permanently set as all packets should
	// fit in one buffer only. Also clear the error bits.
	//
	CurrentDescriptorHi->LanceTMDFlags &=
		~(LANCE_TRANSMIT_MORE_COLLISION |
		LANCE_TRANSMIT_ONE_COLLISION |
		LANCE_TRANSMIT_DEF_ERROR |
		DERR);
	
	CurrentDescriptorHi->TransmitError = 0;
	
	//
	// Set STP and ENP bits.
	//
	CurrentDescriptorHi->LanceTMDFlags |= (STP | ENP);

	// 
	// Toggle OWN bit so chip can start transfer
	// 
	CurrentDescriptorHi->LanceTMDFlags |= OWN;

	// 
	// Start chip now to send packet on the wire
	//
	LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, &Csr0Value);
	Csr0Value &= LANCE_CSR0_IENA;
	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, Csr0Value | LANCE_CSR0_TDMD);

//	Adapter->OpFlags &= ~RESET_PROHIBITED;

	NdisMSendComplete(
					Adapter->LanceMiniportHandle,
					Packet,
					NDIS_STATUS_SUCCESS);

	#if DBG
		if (LanceDbg)
		DbgPrint("<==SendQueuedPacket.\n");
	#endif

}

//jk STATIC
VOID
LanceDeferredSend(
	IN PLANCE_ADAPTER Adapter
	)
{
	PNDIS_PACKET QueuePacket;
	PLANCE_TRANSMIT_DESCRIPTOR_HI CurrentDescriptorHi;

	//
	// Index to next descriptor available.
	//
	USHORT CurrentDescriptorIndex;

	#if DBG
		if (LanceDbg)
		DbgPrint ("==>LanceDeferredSend.\n");
	#endif

	if(Adapter->OpFlags & LANCE_RESET_CALLED)
		return;
	
	Adapter->OpFlags |= LANCE_DEFERRED_SEND_PROCESSING;

	NdisAcquireSpinLock(&Adapter->TxQueueLock);
	while(Adapter->NumPacketsQueued)
	{
//		NdisAcquireSpinLock(&Adapter->TxQueueLock);
		//
		// Check if there is a descriptor available for this request
		// Get the current xmit descriptor.
		//
		
		if(Adapter->OpFlags & LANCE_RESET_CALLED)
			break;

		CurrentDescriptorIndex = Adapter->NextTransmitDescriptorIndex;
		CurrentDescriptorHi = (PLANCE_TRANSMIT_DESCRIPTOR_HI)Adapter->TransmitDescriptorRing + CurrentDescriptorIndex;

		if (CurrentDescriptorHi->LanceTMDFlags & OWN)
		{
			NdisMSetTimer(&(Adapter->DeferredProcessTimer),1);
			break;
		}

		Adapter->LinkActive = TRUE;

		QueuePacket = Adapter->FirstTxQueue;
		Adapter->NumPacketsQueued--;
		DequeuePacket(Adapter->FirstTxQueue, Adapter->LastTxQueue);

		//
		// Increment the next available xit descriptor index.
		//
		if (++Adapter->NextTransmitDescriptorIndex >= TRANSMIT_BUFFERS)
		{
			Adapter->NextTransmitDescriptorIndex = 0;
		}
		NdisReleaseSpinLock(&Adapter->TxQueueLock);

		SendQueuedPacket(Adapter,QueuePacket,CurrentDescriptorIndex);

		NdisAcquireSpinLock(&Adapter->TxQueueLock);
	}
	NdisReleaseSpinLock(&Adapter->TxQueueLock);
	Adapter->OpFlags &= ~LANCE_DEFERRED_SEND_PROCESSING;
		#if DBG
			if (LanceDbg)
			DbgPrint ("<==LanceDeferredSend.\n");
		#endif
}

