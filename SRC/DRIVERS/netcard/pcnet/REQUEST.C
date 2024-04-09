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

    request.c

Abstract:

    This is the code file for the Advanced Micro Devices LANCE
    Ethernet controller.  This driver conforms to the NDIS 3.0 interface.

Environment:

    Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:

	$Log:   V:/pvcs/archives/network/pcnet/homelan/wince/mini4/src/REQUEST.C_y  $
 * 
 *    Rev 1.0   Apr 07 1999 18:12:36   jagannak
 * Initial check-in
 * 
 *    Rev 1.9   Mar 31 1999 12:08:22   cabansag
 *  
 * 
 *    Rev 1.51   10 Jul 1997 18:18:32   steiger
 * Added  support of OID_GEN_LINK_SPEED.
 * 
 * 
--*/

#include <ndis.h>
#include <efilter.h>
#include "lancehrd.h"
#include "lancesft.h"

//for DMI
#ifdef DMI
#include "amdoids.h"
#include "amddmi.h"

#define	TOPOL_OTHER					1
#define	TOPOL_10MB_ETH				2
#define	TOPOL_100MB_ETH			3
#define	TOPOL_10_100MB_ETH		4
#define	TOPOL_100MB_VG_ANYLAN	5
#define	TOPOL_4MB_TOKEN_RING		6
#define	TOPOL_16MB_TOKEN_RING	7
#define	TOPOL_16_4MB_TOKEN_RING	8
#define	TOPOL_2MB_ARCNET			9
#define	TOPOL_20MB_ARCNET			10
#define	TOPOL_FDDI					11
#define	TOPOL_ATM					12
#define	TOPOL_APPLETALK			13

STATIC
BOOLEAN
LanceSuspend(
	PLANCE_ADAPTER Adapter
	);

STATIC
VOID
LanceResume(
	PLANCE_ADAPTER Adapter
	);

STATIC
BOOLEAN
PrepareLanceForPortAccess(
	USHORT RegType,
	PLANCE_ADAPTER Adapter
	);

STATIC
USHORT
LancePortAccess(
	PLANCE_ADAPTER Adapter,
	ULONG * pData,
	USHORT AccessType
	);

STATIC
VOID
DmiRequest(
	PLANCE_ADAPTER Adapter,
	DMIREQBLOCK *ReqBlock
	);
#endif //DMI
// for DMI upto here

STATIC
VOID
LanceChangeClass(
   IN PLANCE_ADAPTER Adapter
   );

STATIC
VOID
LanceChangeAddress(
   IN PLANCE_ADAPTER Adapter
   );

STATIC
UINT
CalculateCRC(
   IN UINT NumberOfBytes,
   IN PCHAR Input
   );

#ifdef NDIS50_PLUS
ULONG
LanceTranslatePattern(
	IN PPMR pPMR,
//	IN PLANCE_ADAPTER Adapter,
   IN PVOID InformationBuffer,
   IN ULONG InformationBufferLength
	);

PLIST_ENTRY
FindFirstFitBlock(
	IN PLANCE_ADAPTER Adapter,
	ULONG	usedSize
	);

PLIST_ENTRY
FindPattern(
	IN PLANCE_ADAPTER Adapter,
	IN PPMR pmr,
	IN ULONG usedSize
	);

NDIS_STATUS
LanceAddWakeUpPattern(
	PLANCE_ADAPTER Adapter,
   IN PVOID InformationBuffer,
   IN ULONG InformationBufferLength
	);

NDIS_STATUS
LanceRemoveWakeUpPattern(
	PLANCE_ADAPTER Adapter,
   IN PVOID InformationBuffer,
   IN ULONG InformationBufferLength
	);

VOID
DefragmentPMR(
	PLANCE_ADAPTER Adapter
	);

NDIS_STATUS
LanceEnableWakeUp(
	PLANCE_ADAPTER Adapter,
   IN PVOID InformationBuffer,
   IN ULONG InformationBufferLength
	);

VOID
LanceEnableMagicPacket(
	PLANCE_ADAPTER Adapter
	);

VOID
LanceEnablePatternMatch(
	PLANCE_ADAPTER Adapter
	);

VOID
LanceEnableLinkChange(
	PLANCE_ADAPTER Adapter
	);

VOID
LanceClearWakeUpMode(
	PLANCE_ADAPTER Adapter
	);

VOID
LanceQueryPWRState(
	PLANCE_ADAPTER Adapter
	);

VOID
LanceSetPWRState(
	PLANCE_ADAPTER Adapter
	);

VOID
LancePMModeInitSetup(
	PLANCE_ADAPTER Adapter
	);

VOID
LancePMModeRecover(
	PLANCE_ADAPTER Adapter
	);

#endif //NDIS50_PLUS

//HomeLan
VOID
HRON(
	PLANCE_ADAPTER Adapter
	);



NDIS_STATUS
LanceQueryInformation(
   IN NDIS_HANDLE MiniportAdapterContext,
   IN NDIS_OID Oid,
   IN PVOID InformationBuffer,
   IN ULONG InformationBufferLength,
   OUT PULONG BytesWritten,
   OUT PULONG BytesNeeded
   )

/*++

Routine Description:

   The LanceQuerylInformation process a Query request for
   NDIS_OIDs that are specific to a binding about the mini-port.

Arguments:

   MiniportAdapterContext - a pointer to the adapter.

   Oid - the NDIS_OID to process.

   InfoBuffer - a pointer into the NdisRequest->InformationBuffer
   into which store the result of the query.

   BytesLeft - the number of bytes left in the InformationBuffer.

   BytesNeeded - If there is not enough room in the information buffer
   then this will contain the number of bytes needed to complete the
   request.

   BytesWritten - a pointer to the number of bytes written into the
   InformationBuffer.

Return Value:

   Status

--*/

{

   PLANCE_ADAPTER Adapter = MiniportAdapterContext;
   PNDIS_OID SupportedOidArray;
   INT SupportedOids;
   PVOID SourceBuffer;
   ULONG SourceBufferLength;
   ULONG GenericUlong;
   USHORT GenericUshort;
   NDIS_STATUS Status;
   UCHAR VendorId[4];
   //jk NDIS_OID MaskOid;
#ifdef NDIS50_PLUS
	NDIS_PNP_CAPABILITIES    Power_Management_Capabilities;
#endif
   static UCHAR VendorDescription[] = "AMD HomeLan Adapter";
   static NDIS_OID LanceGlobalSupportedOids[] = {
                           OID_GEN_SUPPORTED_LIST,
                           OID_GEN_HARDWARE_STATUS,
                           OID_GEN_MEDIA_SUPPORTED,
                           OID_GEN_MEDIA_IN_USE,
#ifdef NDIS40_PLUS
                           OID_GEN_MEDIA_CONNECT_STATUS,
									OID_GEN_MAXIMUM_SEND_PACKETS,
									OID_GEN_VENDOR_DRIVER_VERSION,
#endif
                           OID_GEN_MAXIMUM_LOOKAHEAD,
                           OID_GEN_MAXIMUM_FRAME_SIZE,
                           OID_GEN_MAC_OPTIONS,
                           OID_GEN_PROTOCOL_OPTIONS,
                           OID_GEN_LINK_SPEED,
                           OID_GEN_TRANSMIT_BUFFER_SPACE,
                           OID_GEN_RECEIVE_BUFFER_SPACE,
                           OID_GEN_TRANSMIT_BLOCK_SIZE,
                           OID_GEN_RECEIVE_BLOCK_SIZE,
                           OID_GEN_VENDOR_ID,
                           OID_GEN_VENDOR_DESCRIPTION,
                           OID_GEN_CURRENT_PACKET_FILTER,
                           OID_GEN_CURRENT_LOOKAHEAD,
                           OID_GEN_DRIVER_VERSION,
                           OID_GEN_MAXIMUM_TOTAL_SIZE,
//#ifdef NDIS50_PLUS
//									OID_GEN_SUPPORTED_GUIDS,
//									OID_GEN_NETWORK_LAYER_ADDRESSES,
//#endif
                           OID_GEN_XMIT_OK,
                           OID_GEN_RCV_OK,
                           OID_GEN_XMIT_ERROR,
                           OID_GEN_RCV_ERROR,
                           OID_GEN_RCV_NO_BUFFER,
/*
									OID_GEN_DIRECTED_BYTES_XMIT,     
									OID_GEN_DIRECTED_FRAMES_XMIT,    
									OID_GEN_MULTICAST_BYTES_XMIT,    
									OID_GEN_MULTICAST_FRAMES_XMIT,   
									OID_GEN_BROADCAST_BYTES_XMIT,    
									OID_GEN_BROADCAST_FRAMES_XMIT,   
									OID_GEN_DIRECTED_BYTES_RCV,      
									OID_GEN_DIRECTED_FRAMES_RCV,     
									OID_GEN_MULTICAST_BYTES_RCV,     
									OID_GEN_MULTICAST_FRAMES_RCV,    
									OID_GEN_BROADCAST_BYTES_RCV,     
									OID_GEN_BROADCAST_FRAMES_RCV,    
*/
									OID_GEN_RCV_CRC_ERROR,
									OID_GEN_TRANSMIT_QUEUE_LENGTH,
   
                           OID_802_3_PERMANENT_ADDRESS,
                           OID_802_3_CURRENT_ADDRESS,
                           OID_802_3_MULTICAST_LIST,
                           OID_802_3_MAXIMUM_LIST_SIZE,
                           OID_802_3_RCV_ERROR_ALIGNMENT,
                           OID_802_3_XMIT_ONE_COLLISION,
                           OID_802_3_XMIT_MORE_COLLISIONS,

									OID_802_3_XMIT_DEFERRED,
									OID_802_3_XMIT_MAX_COLLISIONS,
									OID_802_3_RCV_OVERRUN,
									OID_802_3_XMIT_UNDERRUN,
									OID_802_3_XMIT_HEARTBEAT_FAILURE,
									OID_802_3_XMIT_TIMES_CRS_LOST,
									OID_802_3_XMIT_LATE_COLLISIONS

#ifdef NDIS50_PLUS
									,
						        /* powermanagement */
						        OID_PNP_CAPABILITIES,
						        OID_PNP_SET_POWER,
						        OID_PNP_QUERY_POWER,
						        OID_PNP_ADD_WAKE_UP_PATTERN,
						        OID_PNP_REMOVE_WAKE_UP_PATTERN,
						        OID_PNP_ENABLE_WAKE_UP
						        /* end powermanagement */
#endif //NDIS50_PLUS

//for DMI
#ifdef DMI
									,
									OID_CUSTOM_DMI_CI_INIT,
									OID_CUSTOM_DMI_CI_INFO
#endif //DMI
                           };

#ifdef NDIS50_PLUS

/*
typedef struct _GUID
{									// size is 16
	ULONG	Data1;
	USHORT	Data2;
	USHORT	Data3;
	UCHAR	Data4[8];
} GUID;

//
//	Structure to be used for OID_GEN_SUPPORTED_GUIDS.
//	This structure describes an OID to GUID mapping.
//	Or a Status to GUID mapping.
//	When ndis receives a request for a give GUID it will
//	query the miniport with the supplied OID.
//
typedef struct _NDIS_GUID
{
	GUID			Guid;
	union
	{
		NDIS_OID	Oid;
		NDIS_STATUS	Status;
	};
	ULONG		Size;				//	Size of the data element. If the GUID
									//	represents an array then this is the
									//	size of an element in the array.
									//	This is -1 for strings.
	ULONG		Flags;
} NDIS_GUID, *PNDIS_GUID;

#define	fNDIS_GUID_TO_OID			0x00000001
#define	fNDIS_GUID_TO_STATUS		0x00000002
#define	fNDIS_GUID_ANSI_STRING		0x00000004
#define	fNDIS_GUID_UNICODE_STRING	0x00000008
#define	fNDIS_GUID_ARRAY			0x00000010
*/
#define NUM_CUSTOM_GUIDS 4
/*
    static const NDIS_GUID GuidList[NUM_CUSTOM_GUIDS] =
    { { // {F4A80276-23B7-11d1-9ED9-00A0C9010057} example of a uint set
        {   0xf4a80276, 0x23b7, 0x11d1, 0x9e, 0xd9, 0x0, 0xa0, 0xc9, 0x01, 0x00, 0x57 },
            OID_CUSTOM_DRIVER_SET,
            sizeof(ULONG),
            (fNDIS_GUID_TO_OID)
    },
    { // {F4A80277-23B7-11d1-9ED9-00A0C9010057} example of a uint query
        {   0xf4a80277, 0x23b7, 0x11d1, 0x9e, 0xd9, 0x0, 0xa0, 0xc9, 0x01, 0x00, 0x57 },
            OID_CUSTOM_DRIVER_QUERY,
            sizeof(ULONG),
            (fNDIS_GUID_TO_OID)
        },
    { // {F4A80278-23B7-11d1-9ED9-00A0C9010057} example of an array query
        {   0xf4a80278, 0x23b7, 0x11d1, 0x9e, 0xd9, 0x0, 0xa0, 0xc9, 0x01, 0x00, 0x57 },
            OID_CUSTOM_ARRAY,
            sizeof(UCHAR), // size is size of each element in the array!!!!
            (fNDIS_GUID_ARRAY)
        },
    { // {F4A80279-23B7-11d1-9ED9-00A0C9010057} example of a string query
        {   0xf4a80279, 0x23b7, 0x11d1, 0x9e, 0xd9, 0x0, 0xa0, 0xc9, 0x01, 0x00, 0x57 },
            OID_CUSTOM_STRING,
            (ULONG) -1, // according to the docs size is -1 for ANSI or NDIS_STRING string types
            (fNDIS_GUID_ANSI_STRING)
        }
    };
*/

#endif //NDIS50_PLUS


   #ifdef DBG
      if (LanceREQDbg)
         DbgPrint("==>LanceQueryInformation\n");
   #endif

   #ifdef DBG
      if (LancePMDbg)
         DbgPrint("Request ID %x\n",Oid);
   #endif

   SupportedOidArray = (PNDIS_OID)LanceGlobalSupportedOids;
   SupportedOids = sizeof(LanceGlobalSupportedOids)/sizeof(ULONG);

   //
   // Initialize these once, since this is the majority
   // of cases.
   //
   SourceBuffer = (PVOID) &GenericUlong;
   SourceBufferLength = sizeof(ULONG);

   //
   // Set default status
   //
   Status = NDIS_STATUS_SUCCESS;

   //
   // Switch on request type
   //
   switch (Oid & OID_TYPE_MASK) {

      case OID_TYPE_GENERAL_OPERATIONAL:

         switch (Oid) {

            case OID_GEN_MAC_OPTIONS:

               GenericUlong = (ULONG)(NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
                                     NDIS_MAC_OPTION_RECEIVE_SERIALIZED |
                                     NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |
                                     NDIS_MAC_OPTION_NO_LOOPBACK
                                     );

               break;

            case OID_GEN_SUPPORTED_LIST:

               SourceBuffer =  SupportedOidArray;
               SourceBufferLength = sizeof(LanceGlobalSupportedOids);
               break;

            case OID_GEN_HARDWARE_STATUS:

               GenericUlong = (Adapter->OpFlags & RESET_IN_PROGRESS) ?
                           NdisHardwareStatusReset : NdisHardwareStatusReady;

               break;

            case OID_GEN_MEDIA_SUPPORTED:
            case OID_GEN_MEDIA_IN_USE:

               GenericUlong = NdisMedium802_3;
               break;

            case OID_GEN_MAXIMUM_LOOKAHEAD:

               GenericUlong = LANCE_INDICATE_MAXIMUM-MAC_HEADER_SIZE;
               break;

            case OID_GEN_MAXIMUM_FRAME_SIZE:
               //
               // 1514 - 14
               //
               GenericUlong = LANCE_INDICATE_MAXIMUM-MAC_HEADER_SIZE;
// To fix HomeLan BUG
//               GenericUlong = 1000;
               break;

            case OID_GEN_MAXIMUM_TOTAL_SIZE:

               GenericUlong = LANCE_INDICATE_MAXIMUM;
               
               break;

#ifdef NDIS40_PLUS

				case OID_GEN_MEDIA_CONNECT_STATUS:
					if (!Adapter->CableDisconnected)
						GenericUlong = NdisMediaStateConnected;
					else
						GenericUlong = NdisMediaStateDisconnected;
					break;

				case OID_GEN_MAXIMUM_SEND_PACKETS:
					if (Adapter->ActivePhyAddress == HOMERUN_PHY_ADDRESS)
						GenericUlong = MAX_SEND_PACKETS_HL;
					else
						GenericUlong = MAX_SEND_PACKETS;
					break;

				case OID_GEN_VENDOR_DRIVER_VERSION:
               GenericUshort = (LANCE_DRIVER_MAJOR_VERSION << 8) + LANCE_DRIVER_MINOR_VERSION;
               SourceBuffer = &GenericUshort;
               SourceBufferLength = sizeof(USHORT);
					break;
#endif

            case OID_GEN_LINK_SPEED:
					if (Adapter->ActivePhyAddress == HOMERUN_PHY_ADDRESS)
					{
						Adapter->LineSpeed = 1;
						GenericUlong = Adapter->LineSpeed * 10000; // in 100bps units
						break;
					}
					else
					{
						LanceGetActiveMediaInfo (Adapter);
					}
					GenericUlong = Adapter->LineSpeed * 10000; // in 100bps units
               break;

            case OID_GEN_TRANSMIT_BUFFER_SPACE:

               GenericUlong = TRANSMIT_BUFFER_SIZE * TRANSMIT_BUFFERS;
               break;

            case OID_GEN_RECEIVE_BUFFER_SPACE:

               GenericUlong = RECEIVE_BUFFER_SIZE * RECEIVE_BUFFERS;
               break;

            case OID_GEN_TRANSMIT_BLOCK_SIZE:

               GenericUlong = TRANSMIT_BUFFER_SIZE;
               break;

            case OID_GEN_RECEIVE_BLOCK_SIZE:

               GenericUlong = RECEIVE_BUFFER_SIZE;
               break;

            case OID_GEN_VENDOR_ID:

               LANCE_MOVE_MEMORY(VendorId, Adapter->PermanentNetworkAddress, 3);
               VendorId[3] = 0x0;
               SourceBuffer = VendorId;
               SourceBufferLength = sizeof(VendorId);
               break;

            case OID_GEN_VENDOR_DESCRIPTION:

               SourceBuffer = (PVOID)VendorDescription;
               SourceBufferLength = sizeof(VendorDescription);
               break;

            case OID_GEN_DRIVER_VERSION:

               GenericUshort = (LANCE_NDIS_MAJOR_VERSION << 8) + LANCE_NDIS_MINOR_VERSION;
               SourceBuffer = &GenericUshort;
               SourceBufferLength = sizeof(USHORT);
               break;

            case OID_GEN_CURRENT_LOOKAHEAD:
               
               GenericUlong = LANCE_INDICATE_MAXIMUM-MAC_HEADER_SIZE;

               #if DBG
               if (LanceREQDbg)
                  DbgPrint("Enter LanceQueryInformation: OID_GEN_CURRENT_LOOKAHEAD -> %d\n", GenericUlong);
               #endif

               break;

            case OID_GEN_CURRENT_PACKET_FILTER:

               GenericUlong = Adapter->CurrentPacketFilter;
               break;

#ifdef NDIS50_PLUS

				case OID_GEN_SUPPORTED_GUIDS:
					SourceBuffer = (PUCHAR) 0;
					SourceBufferLength = 0;
//					SourceBuffer = (PUCHAR) &GuidList;
//					SourceBufferLength = sizeof(GuidList);
               Status = NDIS_STATUS_NOT_SUPPORTED;
				break;

#endif //NDIS50_PLUS

            default:

               ASSERT(FALSE);
               Status = NDIS_STATUS_NOT_SUPPORTED;
               break;

         }

         break;

      case OID_TYPE_GENERAL_STATISTICS:

         switch (Oid) {
            case OID_GEN_XMIT_OK:

               GenericUlong = Adapter->GeneralMandatory[GM_TRANSMIT_GOOD];
               break;

            case OID_GEN_RCV_OK:

               GenericUlong = Adapter->GeneralMandatory[GM_RECEIVE_GOOD];
               break;

            case OID_GEN_XMIT_ERROR:

               GenericUlong = Adapter->GeneralMandatory[GM_TRANSMIT_BAD];
               break;

            case OID_GEN_RCV_ERROR:

               GenericUlong = Adapter->GeneralMandatory[GM_RECEIVE_BAD];
               break;

            case OID_GEN_RCV_NO_BUFFER:

               GenericUlong = Adapter->GeneralMandatory[GM_RECEIVE_NO_BUFFER];
               break;
/*
The following statistics are not supported. They could be supported by Wrapper,
which is not unknown yet.
If any customer requests those statistics, than we might need to enable them.
However, Bytes related statistics need to be modified to support LargeInteger.				

//				case OID_GEN_DIRECTED_BYTES_XMIT:
//
//               GenericUlong = Adapter->GeneralOptionalByteCount[GO_DIRECTED_TRANSMITS];
//               break;
//
				case OID_GEN_DIRECTED_FRAMES_XMIT:
					GenericUlong = Adapter->GeneralOptionalFrameCount[GO_DIRECTED_TRANSMITS];
					break;

//				case OID_GEN_MULTICAST_BYTES_XMIT:
//               GenericUlong = Adapter->GeneralOptionalByteCount[GO_DIRECTED_TRANSMITS];
//               break;
//
				case OID_GEN_MULTICAST_FRAMES_XMIT:
               GenericUlong = Adapter->GeneralOptionalFrameCount[GO_MULTICAST_TRANSMITS];
               break;

//				case OID_GEN_BROADCAST_BYTES_XMIT:
//               GenericUlong = Adapter->GeneralOptionalByteCount[GO_DIRECTED_TRANSMITS];
//               break;
//
				case OID_GEN_BROADCAST_FRAMES_XMIT:
               GenericUlong = Adapter->GeneralOptionalFrameCount[GO_BROADCAST_TRANSMITS];
               break;

//				case OID_GEN_DIRECTED_BYTES_RCV:
//               GenericUlong = Adapter->GeneralOptionalByteCount[GO_DIRECTED_TRANSMITS];
//               break;
//
				case OID_GEN_DIRECTED_FRAMES_RCV:
               GenericUlong = Adapter->GeneralOptionalFrameCount[GO_DIRECTED_RECEIVES];
               break;

//				case OID_GEN_MULTICAST_BYTES_RCV:
//               GenericUlong = Adapter->GeneralOptionalByteCount[GO_DIRECTED_RECEIVES];
//               break;
//
				case OID_GEN_MULTICAST_FRAMES_RCV:
               GenericUlong = Adapter->GeneralOptionalFrameCount[GO_MULTICAST_RECEIVES];
               break;

//				case OID_GEN_BROADCAST_BYTES_RCV:
//               GenericUlong = Adapter->GeneralOptionalByteCount[GO_DIRECTED_RECEIVES];
//               break;
//
				case OID_GEN_BROADCAST_FRAMES_RCV:
               GenericUlong = Adapter->GeneralOptionalFrameCount[GO_BROADCAST_RECEIVES];
               break;
*/				
				case OID_GEN_RCV_CRC_ERROR:
               GenericUlong = Adapter->GeneralOptional[GO_RECEIVE_CRC-GO_ARRAY_START];
               break;

				case OID_GEN_TRANSMIT_QUEUE_LENGTH:				
					GenericUlong = Adapter->NumPacketsQueued;
               break;

            default:

               ASSERT(FALSE);
               Status = NDIS_STATUS_NOT_SUPPORTED;
               break;
         }
         break;

      case OID_TYPE_802_3_OPERATIONAL:

         switch (Oid) {

            case OID_802_3_PERMANENT_ADDRESS:

               SourceBuffer = Adapter->PermanentNetworkAddress;
               SourceBufferLength = 6;
               break;

            case OID_802_3_CURRENT_ADDRESS:

               SourceBuffer = Adapter->CurrentNetworkAddress;
               SourceBufferLength = 6;
               break;

            case OID_802_3_MAXIMUM_LIST_SIZE:

               GenericUlong = LANCE_MAX_MULTICAST;
               break;

            default:

               ASSERT(FALSE);
               Status = NDIS_STATUS_NOT_SUPPORTED;
               break;

         }

         break;

      case OID_TYPE_802_3_STATISTICS:

         switch (Oid) {

            case OID_802_3_RCV_ERROR_ALIGNMENT:

               GenericUlong = Adapter->MediaMandatory[MM_RECEIVE_ERROR_ALIGNMENT];
               break;

            case OID_802_3_XMIT_ONE_COLLISION:

               GenericUlong = Adapter->MediaMandatory[MM_TRANSMIT_ONE_COLLISION];
               break;

            case OID_802_3_XMIT_MORE_COLLISIONS:

               GenericUlong = Adapter->MediaMandatory[MM_TRANSMIT_MORE_COLLISIONS];
               break;

				case OID_802_3_XMIT_DEFERRED:
               GenericUlong = Adapter->MediaOptional[MO_TRANSMIT_DEFERRED];
               break;

				case OID_802_3_XMIT_MAX_COLLISIONS:
               GenericUlong = Adapter->MediaOptional[MO_TRANSMIT_MAX_COLLISIONS];
               break;

				case OID_802_3_RCV_OVERRUN:
               GenericUlong = Adapter->MediaOptional[MO_RECEIVE_OVERRUN];
               break;

				case OID_802_3_XMIT_UNDERRUN:
               GenericUlong = Adapter->MediaOptional[MO_TRANSMIT_UNDERRUN];
               break;

//
// HeartBeat Error is used to provide HomeRun Phy state
//
				case OID_802_3_XMIT_HEARTBEAT_FAILURE:
//               GenericUlong = Adapter->MediaOptional[MO_TRANSMIT_MAX_COLLISIONS];
					if (Adapter->ActivePhyAddress == HOMERUN_PHY_ADDRESS)
					{
						LANCE_READ_MII(Adapter->MappedIoBaseAddress,
								(Adapter->ActivePhyAddress | HR_PHY_STATUS_REG), &GenericUshort);
						GenericUlong = (ULONG)GenericUshort;
					}
					else
					{
						GenericUlong = 0;
					}
               break;

				case OID_802_3_XMIT_TIMES_CRS_LOST:
               GenericUlong = Adapter->MediaOptional[MO_TRANSMIT_TIMES_CRS_LOST];
               break;

				case OID_802_3_XMIT_LATE_COLLISIONS:
               GenericUlong = Adapter->MediaOptional[MO_TRANSMIT_LATE_COLLISIONS];
               break;

            default:

               ASSERT(FALSE);
               Status = NDIS_STATUS_NOT_SUPPORTED;
               break;

         }

         break;

#ifdef NDIS50_PLUS
		case OID_TYPE_POWER_MANAGEMENT:
			switch (Oid) {
				case OID_PNP_CAPABILITIES:
					#ifdef DBG
					if (LancePMDbg)
					{
						DbgPrint("Query PNP Capabilities\n");
					}
					#endif

					// since we are stubbing power management, return only the
					// D0 power state indicating that the lowest power state we
					// support is D0 (full power)
					if (!(Adapter->FuncFlags & CAP_POWER_MANAGEMENT))
					{
						Status = NDIS_STATUS_NOT_SUPPORTED; //stub
						//break;
						return Status;
					}

					Power_Management_Capabilities.WakeUpCapabilities.MinMagicPacketWakeUp   = NdisDeviceStateD3;
					Power_Management_Capabilities.WakeUpCapabilities.MinPatternWakeUp       = NdisDeviceStateD3;
					Power_Management_Capabilities.WakeUpCapabilities.MinLinkChangeWakeUp    = NdisDeviceStateD3;

					SourceBuffer = (PVOID) &Power_Management_Capabilities;
					SourceBufferLength = sizeof(NDIS_PNP_CAPABILITIES);

					break;

				case OID_PNP_QUERY_POWER:
					// return Success always.
					#ifdef DBG
						if (LancePMDbg)
						{
							DbgPrint("Query Current Power State in QueryRoutine\n");
							DbgPrint("NDIS_PNP_WAKE_UP_MAGIC_PACKET is %i\n",
											NDIS_PNP_WAKE_UP_MAGIC_PACKET);
							DbgPrint("NDIS_PNP_WAKE_UP_PATTERN_MATCH is %i\n",
											NDIS_PNP_WAKE_UP_PATTERN_MATCH);
							DbgPrint("NDIS_PNP_WAKE_UP_LINK_CHANGE is %i\n",
											NDIS_PNP_WAKE_UP_LINK_CHANGE);
						}
					#endif

					if (!(Adapter->FuncFlags & CAP_POWER_MANAGEMENT))
					{
						Status = NDIS_STATUS_NOT_SUPPORTED; //stub
						//break;
						return Status;
					}

					GenericUlong = 0;

					break;

				default :
	         	Status = NDIS_STATUS_NOT_SUPPORTED;
   	      	break;
			}
			break;
#endif //NDIS50_PLUS

#ifdef DMI
		case OID_TYPE_CUSTOM_OPER:

			switch (Oid)
			{
				case OID_CUSTOM_DMI_CI_INIT:
					GenericUlong = *((ULONG *)&(Adapter->DmiIdRev[0]));
					break;

				case OID_CUSTOM_DMI_CI_INFO:
					DmiRequest (Adapter, (DMIREQBLOCK *)InformationBuffer);
					#if DBG
					if (LanceREQDbg)
					{
						DbgPrint("LanceQueryInformation (DMI): Oid = %x\n", Oid);
					}
					#endif
					*BytesWritten = sizeof(DMIREQBLOCK);
					return Status;

				default:
					*BytesWritten = 0;
					break;
			}
			break;
#endif //DMI

      default:

         ASSERT(FALSE);
         Status = NDIS_STATUS_NOT_SUPPORTED;
         break;
   }

   if (Status == NDIS_STATUS_SUCCESS) {

      if (SourceBufferLength > InformationBufferLength) {

         //
         // Not enough room in InformationBuffer. Punt
         //
         *BytesNeeded = SourceBufferLength;
         Status = NDIS_STATUS_INVALID_LENGTH;

      } else {

         //
         // Store result.
         //
         LANCE_MOVE_MEMORY(InformationBuffer, SourceBuffer, SourceBufferLength);
         *BytesWritten = SourceBufferLength;

         #if DBG
            if (LanceREQDbg) {
               DbgPrint("LanceRequeryInformation: Oid = %x\n", Oid);
               DbgPrint("LanceRequeryInformation: return %x\n", *(PULONG)SourceBuffer);
            }
         #endif

      }
   }

   #if DBG
      if (LanceREQDbg)
         DbgPrint("<==LanceQueryInformation\n");
   #endif

   return Status;
}



NDIS_STATUS
LanceSetInformation(
   IN NDIS_HANDLE MiniportAdapterContext,
   IN NDIS_OID Oid,
   IN PVOID InformationBuffer,
   IN ULONG InformationBufferLength,
   OUT PULONG BytesRead,
   OUT PULONG BytesNeeded
   )

/*++

Routine Description:

   The LanceSetInformation handles a set operation for a
   single Oid

Arguments:

   MiniportAdapterContext - a pointer to the adapter.

   Oid - The OID of the set.

   InformationBuffer - Holds the data to be set.

   InformationBufferLength - The length of InformationBuffer.

   BytesRead - If the call is successful, returns the number
       of bytes read from InformationBuffer.

   BytesNeeded - If there is not enough data in InformationBuffer
       to satisfy the OID, returns the amount of storage needed.

Return Value:

   NDIS_STATUS_SUCCESS
   NDIS_STATUS_PENDING
   NDIS_STATUS_INVALID_LENGTH
   NDIS_STATUS_INVALID_OID

--*/

{

   PLANCE_ADAPTER Adapter = MiniportAdapterContext;
   PUCHAR InfoBuffer = (PUCHAR)InformationBuffer;
   ULONG PacketFilter;
	NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
	//jk USHORT Flag;
   UCHAR NewAddressCount = (UCHAR)(InformationBufferLength / ETH_LENGTH_OF_ADDRESS);
	//jk USHORT Data;
	//jk BOOLEAN IsCancelled;

   #if DBG
      if (LanceREQDbg){
         DbgPrint("==>LanceSetInformation\n");
         DbgPrint("LanceSetInformation: Oid = %x\n", Oid);
      }
   #endif

   #ifdef DBG
      if (LancePMDbg)
//			if (OID_TYPE_POWER_MANAGEMENT == (Oid & OID_TYPE_MASK))
         DbgPrint("Request ID %x\n",Oid);
   #endif

   //
   // Check for the most common OIDs
   //
   switch (Oid) {
   
      case OID_802_3_MULTICAST_LIST:
         //
         // Verify length
         //
         if ((InformationBufferLength % ETH_LENGTH_OF_ADDRESS) != 0) {
            
            *BytesRead = 0;
            *BytesNeeded = 0;
            return NDIS_STATUS_INVALID_LENGTH;

         }

         //
         // Verify number of new multicast addresses
         //
         if (NewAddressCount > LANCE_MAX_MULTICAST) {

            *BytesRead = 0;
            *BytesNeeded = 0;
            return NDIS_STATUS_MULTICAST_FULL;

         }

         //
         // Save multicast address information
         //
         Adapter->NumberOfAddresses = NewAddressCount;

         LANCE_MOVE_MEMORY ((PVOID)Adapter->MulticastAddresses, InfoBuffer, InformationBufferLength);

			if (Adapter->CurrentPacketFilter & (NDIS_PACKET_TYPE_ALL_MULTICAST |
															NDIS_PACKET_TYPE_PROMISCUOUS))
			{

		   #if DBG
      		if (LanceFilterDbg)
		         DbgPrint("Returned since current filter is %x\n",Adapter->CurrentPacketFilter);
		   #endif
				return NDIS_STATUS_SUCCESS;
			}

#ifdef NDIS50_PLUS
			if (Adapter->OpFlags & PM_MODE)
			{
				LancePMModeRecover(Adapter);
				Adapter->WakeUpMode = 0;
				NdisMSetPeriodicTimer (&(Adapter->CableTimer),CABLE_CHK_TIMEOUT);
				Adapter->OpFlags &= ~PM_MODE;
			}
#endif
         //
         // Change init-block with new multicast addresses
         //

		   #if DBG
      		if (LanceFilterDbg)
		         DbgPrint("New multicast address count is %i\n",NewAddressCount);
		   #endif
         LanceChangeAddress(Adapter);

         //
         // Reload init-block
         //
			#ifdef DBG
				if (LanceInitDbg||LanceFilterDbg)
				{
					DbgPrint("MulticastList calls LanceInit, %i\n",Adapter->NumberOfAddresses);
				}
			#endif
			LanceInit(Adapter, FALSE);

         //
         // Set return values
         //
         *BytesRead = InformationBufferLength;

         break;

      case OID_GEN_CURRENT_PACKET_FILTER:
         
         //
         // Verify length
         //
         if (InformationBufferLength != 4) {

            *BytesNeeded = 4;
            *BytesRead = 0;
            return NDIS_STATUS_INVALID_LENGTH;

         }

         //
         // Now call the filter package to set the packet filter.
         //
         LANCE_MOVE_MEMORY ((PVOID)&PacketFilter, InfoBuffer, sizeof(ULONG));

         //
         // Verify bits
         //
         if (PacketFilter & (NDIS_PACKET_TYPE_SOURCE_ROUTING |
                           NDIS_PACKET_TYPE_SMT |
                           NDIS_PACKET_TYPE_MAC_FRAME |
                           NDIS_PACKET_TYPE_FUNCTIONAL |
                           NDIS_PACKET_TYPE_ALL_FUNCTIONAL |
                           NDIS_PACKET_TYPE_GROUP
                           )) {

            *BytesRead = 4;
            *BytesNeeded = 0;
		   #if DBG
      		if (LanceFilterDbg)
		         DbgPrint("Not Supported Filter type %x\n",PacketFilter);
		   #endif
            return NDIS_STATUS_NOT_SUPPORTED;

         }
   
#ifdef NDIS50_PLUS
			if (Adapter->OpFlags & PM_MODE)
			{
				LancePMModeRecover(Adapter);
				Adapter->WakeUpMode = 0;
				NdisMSetPeriodicTimer (&(Adapter->CableTimer),CABLE_CHK_TIMEOUT);
				Adapter->OpFlags &= ~PM_MODE;
			}
#endif
         //
         // Save new packet filter
         //
         Adapter->CurrentPacketFilter = PacketFilter;

         //
         // Change init-block with new filter
         //
		   #if DBG
      		if (LanceFilterDbg)
		         DbgPrint("New Filter Type %x\n",PacketFilter);
		   #endif
         LanceChangeClass(Adapter);

         //
         // Reload init-block
         //
			#ifdef DBG
				if (LanceInitDbg)
				{
					DbgPrint("PacketFilter calls LanceInit\n");
				}
			#endif
			LanceInit(Adapter, FALSE);

         //
         // Set return values
         //
         *BytesRead = InformationBufferLength;

         break;

      case OID_GEN_CURRENT_LOOKAHEAD:
		
         //
         // No need to record requested lookahead length since we
         // always indicate the whole packet.
         //
         *BytesRead = 4;

         break;
#ifdef NDIS50_PLUS

		case OID_GEN_NETWORK_LAYER_ADDRESSES:
/*
         if (InformationBufferLength != ETH_LENGTH_OF_ADDRESS)
			{
            *BytesRead = 0;
            *BytesNeeded = 0;
            return NDIS_STATUS_INVALID_LENGTH;
         }

			NdisMoveMemory(
				Adapter->CurrentNetworkAddress,
				(InformationBuffer != NULL) ? InformationBuffer : Adapter->PermanentNetworkAddress,
				ETH_LENGTH_OF_ADDRESS);

			LanceInit(Adapter,FALSE);
*/
         //
         // Set return values
         //
         *BytesRead = InformationBufferLength;

			break;


		// stub the next four cases to set up power management stuff
		// if the adapter had supported power management, the commented lines
		// should replace the stub lines. See the power management spec

		case OID_PNP_QUERY_POWER:
			// return Success always.
			#ifdef DBG
				if (LancePMDbg)
				{
					DbgPrint("Query Current Power State in Set Routine\n");
					DbgPrint("NDIS_PNP_WAKE_UP_MAGIC_PACKET is %i\n",
									NDIS_PNP_WAKE_UP_MAGIC_PACKET);
					DbgPrint("NDIS_PNP_WAKE_UP_PATTERN_MATCH is %i\n",
									NDIS_PNP_WAKE_UP_PATTERN_MATCH);
					DbgPrint("NDIS_PNP_WAKE_UP_LINK_CHANGE is %i\n",
									NDIS_PNP_WAKE_UP_LINK_CHANGE);
				}
			#endif

			break;

		case OID_PNP_SET_POWER:
			/*
				Called whenever there is a power level change.
				So, depending on the input power, prepare for the given power state
				and program the chip.
				For e.g., if power state is 1(D0), then restart the chip.
				If power state is 4(D3), then program the MAC chip in power management
				mode and Disable the interrupt.(Eventhough the interrupt got disabled,
				InterruptHandler might be called. Therefore, prepare for the call.)
				Used mode (Pattern match, Link Change Detection, and/or Magic Packet) is
				already defined by OID_PNP_ENABLE_WAKE_UP.
			*/
 
			#ifdef DBG
				if (LancePMDbg)
				{
					DbgPrint("Power State change to %i\n",*(PULONG)InformationBuffer);
				}
			#endif

			if (!(Adapter->FuncFlags & CAP_POWER_MANAGEMENT))
			{
				Status = NDIS_STATUS_NOT_SUPPORTED; //stub
				break;
			}
			Adapter->powerState = *(PNDIS_DEVICE_POWER_STATE)InformationBuffer;
		   *BytesRead = InformationBufferLength;

			if (Adapter->powerState == 1)
			{

			#ifdef DBG
				if (LancePMDbg)
				{
					LANCE_READ_CSR (Adapter->MappedIoBaseAddress, 0, &Data);
					DbgPrint("CSR0 is %x\n",Data);
//					LANCE_READ_CSR (Adapter->MappedIoBaseAddress, 116, &Data);
//					DbgPrint("CSR116 is %x\n",Data);
				}
			#endif

				LancePMModeRecover(Adapter);
				Adapter->WakeUpMode = 0;
				NdisMSetPeriodicTimer (&(Adapter->CableTimer),CABLE_CHK_TIMEOUT);
				Adapter->OpFlags &= ~PM_MODE;
				
			#ifdef DBG
				if (LancePMDbg)
				{
					LANCE_READ_CSR (Adapter->MappedIoBaseAddress, 0, &Data);
					DbgPrint("CSR0 is %x\n",Data);
				}
			#endif
			}	
			else
			{
				// Set this flag so that Interrupt hadler does nothing at this time.
				Adapter->OpFlags |= PM_MODE;

				NdisMCancelTimer (&(Adapter->CableTimer),&IsCancelled);
				Adapter->HLAC.RcvStateValid = FALSE;

				LancePMModeInitSetup(Adapter);

				#ifdef DBG
					if (LancePMDbg)
					{
					LANCE_READ_CSR(Adapter->MappedIoBaseAddress, 116, &Data);
					DbgPrint("CSR116 is %x\n",Data);
					}
				#endif

				if (Adapter->WakeUpMode & NDIS_PNP_WAKE_UP_MAGIC_PACKET)
					LanceEnableMagicPacket(Adapter);

				if (Adapter->WakeUpMode & NDIS_PNP_WAKE_UP_PATTERN_MATCH)
					LanceEnablePatternMatch(Adapter);

				if (Adapter->WakeUpMode & NDIS_PNP_WAKE_UP_LINK_CHANGE)
					LanceEnableLinkChange(Adapter);

			#ifdef DBG
				if (LancePMDbg)
				{
					LANCE_READ_CSR (Adapter->MappedIoBaseAddress, 0, &Data);
					DbgPrint("CSR0 is %x\n",Data);
					LANCE_READ_CSR (Adapter->MappedIoBaseAddress, 116, &Data);
					DbgPrint("CSR116 is %x\n",Data);
					LANCE_READ_CSR (Adapter->MappedIoBaseAddress, 15, &Data);
					DbgPrint("CSR15 is %x\n",Data);
					LANCE_READ_CSR (Adapter->MappedIoBaseAddress, 5, &Data);
					DbgPrint("CSR5 is %x\n",Data);
					LANCE_READ_BCR (Adapter->MappedIoBaseAddress, 2, &Data);
					DbgPrint("BCR2 is %x\n",Data);
					LANCE_READ_CSR (Adapter->MappedIoBaseAddress, 12, &Data);
					DbgPrint("CSR12 is %x\n",Data);
					LANCE_READ_CSR (Adapter->MappedIoBaseAddress, 13, &Data);
					DbgPrint("CSR13 is %x\n",Data);
					LANCE_READ_CSR (Adapter->MappedIoBaseAddress, 14, &Data);
					DbgPrint("CSR14 is %x\n",Data);
				}
			#endif

				//
				// Set START bit in CSR0 to 1. This is a must!
				//
				LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, 0x0002);

			}

		   break;

		case OID_PNP_ADD_WAKE_UP_PATTERN:
		   // call a function that would program the adapter's wake
		   // up pattern, return success

			#ifdef DBG
				if (LancePMDbg)
				{
					DbgPrint("ADD WakeUpPattern, size of %i\n",InformationBufferLength);
				}
			#endif

			if (!(Adapter->FuncFlags & CAP_POWER_MANAGEMENT))
			{
			   *BytesRead = 0;                     //stub
				Status = NDIS_STATUS_NOT_SUPPORTED; //stub
				break;
			}

		   *BytesRead = InformationBufferLength;
			Status = LanceAddWakeUpPattern(Adapter,
													InformationBuffer,
													InformationBufferLength);	
		   //Status = NDIS_STATUS_NOT_SUPPORTED; //stub
			#ifdef DBG
				if (LancePMDbg)
				{
					DbgPrint("Add WakeUpPattern returned %x\n",Status);
				}
			#endif
		   break;

		case OID_PNP_REMOVE_WAKE_UP_PATTERN:
			#ifdef DBG
				if (LancePMDbg)
				{
					DbgPrint("Remove WakeUpPattern, size of %i\n",InformationBufferLength);
				}
			#endif
		   // call a function that would remove the adapter's wake
		   // up pattern, return success
			if (!(Adapter->FuncFlags & CAP_POWER_MANAGEMENT))
			{
			   *BytesRead = 0;                     //stub
				Status = NDIS_STATUS_NOT_SUPPORTED; //stub
				break;
			}

		   *BytesRead = InformationBufferLength;
			Status = LanceRemoveWakeUpPattern(Adapter,
														InformationBuffer,
														InformationBufferLength);	
			#ifdef DBG
				if (LancePMDbg)
				{
					DbgPrint("Remove WakeUpPattern returned %x\n",Status);
				}
			#endif
		   break;

		case OID_PNP_ENABLE_WAKE_UP:
			#ifdef DBG
				if (LancePMDbg)
				{
					DbgPrint("EnableWakeUp Call of %x\n",*(PULONG)InformationBuffer);
				}
			#endif

		   // call a function that would enable wake up on the adapter
		   // return success
			if (!(Adapter->FuncFlags & CAP_POWER_MANAGEMENT))
			{
			   *BytesRead = 0;                     //stub
				Status = NDIS_STATUS_NOT_SUPPORTED; //stub
				break;
			}

		   *BytesRead = InformationBufferLength;
			Status = LanceEnableWakeUp(Adapter,
												InformationBuffer,
												InformationBufferLength);	

			#ifdef DBG
				if (LancePMDbg)
				{
					DbgPrint("WakeUpMode is %x\n",Adapter->WakeUpMode);
				}
			#endif
		   break;

#endif //NDIS50_PLUS

      default:

         *BytesRead = 0;
         *BytesNeeded = 0;
         return NDIS_STATUS_INVALID_OID;

   }

   #if DBG
      if (LanceREQDbg)
         DbgPrint("<==LanceSetInformation\n");
   #endif

   return Status;

}



STATIC
VOID
LanceChangeClass(
   IN PLANCE_ADAPTER Adapter
   )

/*++

Routine Description:

   Modifies the mode register and logical address filter of Lance.

Arguments:

   Adapter - The pointer to the adapter

Return Value:

   None.

--*/

{

   //
   // Local Pointer to the Initialization Block.
   //
   //jk PLANCE_INIT_BLOCK InitializationBlock;
   PLANCE_INIT_BLOCK_HI InitializationBlockHi;

   #if DBG
      if (LanceFilterDbg){
         DbgPrint("==>LanceChangeClass\n");
         DbgPrint("ChangeClass: CurrentPacketFilter = %x\n",
                     Adapter->CurrentPacketFilter);
      }
   #endif
	
	InitializationBlockHi = (PLANCE_INIT_BLOCK_HI)Adapter->InitializationBlock;

   if (Adapter->CurrentPacketFilter & NDIS_PACKET_TYPE_PROMISCUOUS) {

      #if DBG
         if (LanceFilterDbg)
            DbgPrint("ChangeClass: Go promiscious.\n");
      #endif

		InitializationBlockHi->Mode = LANCE_PROMISCIOUS_MODE;
   } else {

      USHORT i;

		InitializationBlockHi->Mode = LANCE_NORMAL_MODE;

      if (Adapter->CurrentPacketFilter & NDIS_PACKET_TYPE_ALL_MULTICAST) {

         #if DBG
            if (LanceFilterDbg)
               DbgPrint("ChangeClass: Receive all multicast.\n");
         #endif

			for (i=0; i<8; i++)
				InitializationBlockHi->LogicalAddressFilter[i] = 0xFF;

      } else if (Adapter->CurrentPacketFilter & NDIS_PACKET_TYPE_MULTICAST) {

         #if DBG
            if (LanceFilterDbg)
               DbgPrint("ChangeClass: Receive selected multicast.\n");
         #endif

         //
         // Change address
         //
         LanceChangeAddress(Adapter);

      }
   }

   #if DBG
      if (LanceDbg)
         DbgPrint("<==LanceChangeClass\n");
   #endif
}



STATIC
VOID
LanceChangeAddress(
   IN PLANCE_ADAPTER Adapter
   )

/*++

Routine Description:

   Modifies the logical address filter.

Arguments:

   Adapter - The pointer to the adapter

Return Value:

   None.

--*/

{

   UINT i, j;

   //
   // Local Pointer to the Initialization Block.
   //
//   PLANCE_INIT_BLOCK InitializationBlock;
   PLANCE_INIT_BLOCK_HI InitializationBlockHi;

   #if DBG
      if (LanceDbg)
         DbgPrint("==>LanceChangeAddress\n");
   #endif

	InitializationBlockHi = (PLANCE_INIT_BLOCK_HI)Adapter->InitializationBlock;

	if (Adapter->ActivePhyAddress == HOMERUN_PHY_ADDRESS)
	{
		for (i=0; i<8; i++)
			  InitializationBlockHi->LogicalAddressFilter[i] = 0xff;
	}
	else
	{
		for (i=0; i<8; i++)
			  InitializationBlockHi->LogicalAddressFilter[i] = 0;

	   //
	   // Loop through, copying the addresses into the CAM.
	   //

	   for (i = 0; i < Adapter->NumberOfAddresses; i++) {

		  UCHAR HashCode;
		  UCHAR FilterByte;
		  UINT CRCValue;

		  HashCode = 0;

		  //
		  // Calculate CRC value
		  //
		  CRCValue = CalculateCRC(ETH_LENGTH_OF_ADDRESS,
								  Adapter->MulticastAddresses[i]
								  );

		  for (j = 0; j < 6; j++)
			 HashCode = (HashCode << 1) + (((UCHAR)CRCValue >> j) & 0x01);

		  //
		  // Bits 3-5 of HashCode point to byte in address filter.
		  // Bits 0-2 point to bit within that byte.
		  //
		  FilterByte = HashCode >> 3;

			InitializationBlockHi->LogicalAddressFilter[FilterByte] |=
					  (1 << (HashCode & 0x07));

			}
	}
   #if DBG
      if (LanceDbg)
         DbgPrint("<==LanceChangeAddress\n");
   #endif
}



STATIC
UINT
CalculateCRC(
   IN UINT NumberOfBytes,
   IN PCHAR Input
   )

/*++

Routine Description:

   Calculates a 32 bit CRC value over the input number of bytes.

   NOTE: ZZZ This routine assumes UINTs are 32 bits.

Arguments:

   NumberOfBytes - The number of bytes in the input.

   Input - An input "string" to calculate a CRC over.

Return Value:

   A 32 bit CRC value.

--*/

{

   const UINT POLY = 0x04c11db6;
   UINT CRCValue = 0xffffffff;

   ASSERT(sizeof(UINT) == 4);

   #if DBG
      if (LanceDbg)
         DbgPrint("==>CalculateCRC\n");
   #endif

   for (; NumberOfBytes; NumberOfBytes--) {

      UINT CurrentBit;
      UCHAR CurrentByte = *Input;
      Input++;

      for (CurrentBit = 8; CurrentBit; CurrentBit--) {

         UINT CurrentCRCHigh = CRCValue >> 31;

         CRCValue <<= 1;

         if (CurrentCRCHigh ^ (CurrentByte & 0x01)) {

            CRCValue ^= POLY;
            CRCValue |= 0x00000001;

         }

         CurrentByte >>= 1;

      }
   }

   #if DBG
      if (LanceDbg)
         DbgPrint("<==CalculateCRC\n");
   #endif

   return CRCValue;

}

//For DMI
#ifdef DMI

STATIC
VOID
DmiRequest(
	PLANCE_ADAPTER Adapter,
	DMIREQBLOCK *ReqBlock
	)
/*++
	   
Routine Description:

	Action routine that will get called when a custom OID for DMI
	instrumentation is requested. This routine receives the Dmi Request 
	Block structure and fills it up. 

Arguments:

	ReqBlock - Pointer to the Request Block Structure passed on by
	the Component Interface.

Return Value:

	None.

--*/

{	
	//
	// Counter.
	//

	USHORT i;

	//
	// Status to return.
	//

	NDIS_STATUS Status;

	//
	// The number of addresses in the Multicast List.
	//

	UINT AddressCount;
	
	//
	// The Query writes addresses currently associated
	// with the Netcard.
	//

	CHAR AddressBuffer[LANCE_MAX_MULTICAST][ETH_LENGTH_OF_ADDRESS];

	//
	// Index to the Multicast List.
	//

	UCHAR index = (UCHAR)ReqBlock->Value;

	//
	// Data for SRAM size
	//
	UCHAR	Data;
	
	ReqBlock->Status = DMI_REQ_SUCCESS;
	switch (ReqBlock->Opcode)
	{

		case DMI_OPCODE_GET_PERM_ADDRESS:
			for (i = 0; i < 6; i++)
			{
				ReqBlock->addr[i] = Adapter->PermanentNetworkAddress[i];
			}
			break;

		case DMI_OPCODE_GET_CURR_ADDRESS:
			for (i = 0; i < 6; i++)
			{
				ReqBlock->addr[i] = Adapter->CurrentNetworkAddress[i];
			}
			break;

		case DMI_OPCODE_GET_TOTAL_TX_PKTS:
			ReqBlock->Counter = Adapter->GeneralMandatory[GM_TRANSMIT_GOOD];
			break;

		case DMI_OPCODE_GET_TOTAL_TX_BYTES:
			ReqBlock->Counter = Adapter->DmiSpecific[DMI_TX_BYTES];
			break;

		case DMI_OPCODE_GET_TOTAL_RX_PKTS:
			ReqBlock->Counter = Adapter->GeneralMandatory[GM_RECEIVE_GOOD];
			break;

		case DMI_OPCODE_GET_TOTAL_RX_BYTES:
			ReqBlock->Counter = Adapter->DmiSpecific[DMI_RX_BYTES];
			break;

		case DMI_OPCODE_GET_TOTAL_TX_ERRS:
			ReqBlock->Counter = Adapter->GeneralMandatory[GM_TRANSMIT_BAD]+
			Adapter->DmiSpecific[DMI_CSR0_BABL]+
			Adapter->MediaOptional[MO_TRANSMIT_HEARTBEAT_FAILURE];
			break;

		case DMI_OPCODE_GET_TOTAL_RX_ERRS:
			ReqBlock->Counter = Adapter->GeneralMandatory[GM_RECEIVE_BAD]+
			Adapter->GeneralMandatory[GM_RECEIVE_NO_BUFFER];
			break;

		case DMI_OPCODE_GET_TOTAL_HOST_ERRS:
			ReqBlock->Counter = Adapter->DmiSpecific[DMI_CSR0_BABL]+
			Adapter->GeneralMandatory[GM_RECEIVE_NO_BUFFER]+
			Adapter->DmiSpecific[DMI_CSR0_MERR]+
			Adapter->MediaOptional[MO_RECEIVE_OVERRUN]+
			Adapter->DmiSpecific[DMI_TXBUFF_ERR]+
			Adapter->DmiSpecific[DMI_RXBUFF_ERR]+
			Adapter->MediaOptional[MO_TRANSMIT_DEFERRED]+
			Adapter->MediaOptional[MO_TRANSMIT_UNDERRUN];
			break;

		case DMI_OPCODE_GET_TOTAL_WIRE_ERRS:
			ReqBlock->Counter = Adapter->MediaOptional[MO_TRANSMIT_HEARTBEAT_FAILURE]+
			Adapter->MediaMandatory[MM_RECEIVE_ERROR_ALIGNMENT]+
			Adapter->GeneralOptional[GO_RECEIVE_CRC-GO_ARRAY_START]+
			Adapter->MediaMandatory[MM_TRANSMIT_MORE_COLLISIONS]+
			Adapter->MediaOptional[MO_TRANSMIT_LATE_COLLISIONS]+
			Adapter->MediaOptional[MO_TRANSMIT_TIMES_CRS_LOST]+
			Adapter->MediaOptional[MO_TRANSMIT_MAX_COLLISIONS];
			break;

		case DMI_OPCODE_GET_MULTI_ADDRESS:
			#if DBG
			if (LanceDbg)
				DbgPrint("Multicast List: \n");
			#endif

			if (index >= Adapter->NumberOfAddresses)
			{
				ReqBlock->addr[7] = 0x55;
				break;
			}
			else
				ReqBlock->addr[7] = 0x00;

			for(i=0; i < ETH_LENGTH_OF_ADDRESS; i ++)
			{
				ReqBlock->addr[i] = (UCHAR)Adapter->MulticastAddresses[index][i];
			}

			#if DBG
			if(LanceDbg)
			{
				DbgPrint("Multicast List: %.x-%.x-%.x-%.x-%.x-%.x\n", 
						(UCHAR)AddressBuffer[index][0],
						(UCHAR)AddressBuffer[index][1],
						(UCHAR)AddressBuffer[index][2],
						(UCHAR)AddressBuffer[index][3],
						(UCHAR)AddressBuffer[index][4],
						(UCHAR)AddressBuffer[index][5]);
			}
			#endif
			break;

		case DMI_OPCODE_GET_DRIVER_VERSION:
			ReqBlock->addr[BUILD_VERS] = 0x00;	// Internal
			ReqBlock->addr[MINOR_VERS] = LANCE_DRIVER_MINOR_VERSION;
			ReqBlock->addr[MAJOR_VERS] = LANCE_DRIVER_MAJOR_VERSION;
			break;

		case DMI_OPCODE_GET_DRIVER_NAME:
			NdisMoveMemory(ReqBlock->addr,LANCE_DRIVER_NAME,sizeof(LANCE_DRIVER_NAME));
			break;

		case DMI_OPCODE_GET_TX_DUPLEX:
/*			switch (Adapter->FullDuplex)
			{
				case FDUP_AUI:
				case FDUP_10BASE_T:
					ReqBlock->Value = 0x02;
					break;

				default:
					ReqBlock->Value = 0x01;
					break;
			}
*/
			ReqBlock->Value = 0x01;
			break;

		case DMI_OPCODE_GET_OP_STATE:
			ReqBlock->Value = 0x5555;
			break;

		case DMI_OPCODE_GET_USAGE_STATE:
			ReqBlock->Value = 0x5555;
			break;

		case DMI_OPCODE_GET_AVAIL_STATE:
			ReqBlock->Value = 0x5555;
			break;

		case DMI_OPCODE_GET_ADMIN_STATE:
			ReqBlock->Value = 0x5555;
			break;

		case DMI_OPCODE_GET_FATAL_ERR_COUNT:
			ReqBlock->Value = 0;
			break;

		case DMI_OPCODE_GET_MAJ_ERR_COUNT:
			ReqBlock->Value = 0;
			break;

		case DMI_OPCODE_GET_WRN_ERR_COUNT:
			ReqBlock->Counter = Adapter->DmiSpecific[DMI_RESET_CNT];
			break;

		case DMI_OPCODE_GET_BOARD_FOUND:
#if 0
			switch (Adapter->BoardFound)
			{
				case PCI_DEV:
					ReqBlock->Value = DMI_PCI_BOARD;
					break;

				case PLUG_PLAY_DEV:
					ReqBlock->Value = DMI_PLUG_PLAY_BOARD;
					break;

				case LOCAL_DEV:
					ReqBlock->Value = DMI_LOCAL_BOARD;
					break;

				case ISA_DEV:
					ReqBlock->Value = DMI_ISA_BOARD;
					break;

				default:
					ReqBlock->Value = DMI_NO_BOARD;
					break;
			}
#endif //0
			ReqBlock->Value = DMI_PCI_BOARD;
			break;

		case DMI_OPCODE_GET_IO_BASE_ADDR:
			ReqBlock->Value = Adapter->MappedIoBaseAddress;
			break;

		case DMI_OPCODE_GET_GET_IRQ:
			ReqBlock->Value = Adapter->LanceInterruptVector;
			break;

		case DMI_OPCODE_GET_DMA_CHANNEL:
			ReqBlock->Value = Adapter->LanceDmaChannel;
			break;

		case DMI_OPCODE_GET_CSR_VALUE:
			ReqBlock->Status = LancePortAccess (Adapter, &ReqBlock->Value, CSR_READ);
			break;

		case DMI_OPCODE_GET_BCR_VALUE:
			ReqBlock->Status = LancePortAccess (Adapter, &ReqBlock->Value, BCR_READ);
			break;

		case DMI_OPCODE_SET_CSR_VALUE:
			ReqBlock->Status = LancePortAccess (Adapter, &ReqBlock->Value, CSR_WRITE);
			break;

		case DMI_OPCODE_SET_BCR_VALUE:
			ReqBlock->Status = LancePortAccess (Adapter, &ReqBlock->Value, BCR_WRITE);
			break;

		case DMI_OPCODE_GET_CSR_NUMBER:
			ReqBlock->Value = Adapter->CsrNum;
			break;

		case DMI_OPCODE_GET_BCR_NUMBER:
			ReqBlock->Value = Adapter->BcrNum;
			break;

		case DMI_OPCODE_SET_CSR_NUMBER:
			Adapter->CsrNum = ReqBlock->Value;
			break;

		case DMI_OPCODE_SET_BCR_NUMBER:
			Adapter->BcrNum = ReqBlock->Value;
			break;

		case DMI_OPCODE_GET_CSR0_ERR: 		// Summary of CSR0 ERR bit set.
			ReqBlock->Counter = Adapter->DmiSpecific[DMI_CSR0_ERR];
			break;

		case DMI_OPCODE_GET_CSR0_BABL:
			ReqBlock->Counter = Adapter->DmiSpecific[DMI_CSR0_BABL];
			break;

		case DMI_OPCODE_GET_CSR0_CERR:
			ReqBlock->Counter = Adapter->MediaOptional[MO_TRANSMIT_HEARTBEAT_FAILURE];
			break;

		case DMI_OPCODE_GET_CSR0_MISS:
			ReqBlock->Counter = Adapter->GeneralMandatory[GM_RECEIVE_NO_BUFFER];
			break;

		case DMI_OPCODE_GET_CSR0_MERR:
			ReqBlock->Counter = Adapter->DmiSpecific[DMI_CSR0_MERR];
			break;

		case DMI_OPCODE_GET_TX_DESC_ERR:
			ReqBlock->Counter = Adapter->GeneralMandatory[GM_TRANSMIT_BAD];
			break;

		case DMI_OPCODE_GET_TX_DESC_MORE:
			ReqBlock->Counter = Adapter->MediaMandatory[MM_TRANSMIT_MORE_COLLISIONS];
			break;

		case DMI_OPCODE_GET_TX_DESC_ONE:
			ReqBlock->Counter = Adapter->MediaMandatory[MM_TRANSMIT_ONE_COLLISION];
			break;

		case DMI_OPCODE_GET_TX_DESC_DEF:
			ReqBlock->Counter = Adapter->MediaOptional[MO_TRANSMIT_DEFERRED];
			break;

		case DMI_OPCODE_GET_TX_DESC_BUF:
			ReqBlock->Counter = Adapter->DmiSpecific[DMI_TXBUFF_ERR];
			break;

		case DMI_OPCODE_GET_TX_DESC_UFLO:
			ReqBlock->Counter = Adapter->MediaOptional[MO_TRANSMIT_UNDERRUN];
			break;

		case DMI_OPCODE_GET_TX_DESC_EXDEF:
			ReqBlock->Counter = Adapter->DmiSpecific[DMI_EX_DEFER];
			break;

		case DMI_OPCODE_GET_TX_DESC_LCOL:
			ReqBlock->Counter = Adapter->MediaOptional[MO_TRANSMIT_LATE_COLLISIONS];
			break;

		case DMI_OPCODE_GET_TX_DESC_LCAR:
			ReqBlock->Counter = Adapter->MediaOptional[MO_TRANSMIT_TIMES_CRS_LOST];
			break;

		case DMI_OPCODE_GET_TX_DESC_RTRY:
			ReqBlock->Counter = Adapter->MediaOptional[MO_TRANSMIT_MAX_COLLISIONS];
			break;

		case DMI_OPCODE_GET_RX_DESC_ERR:
			ReqBlock->Counter = Adapter->GeneralMandatory[GM_RECEIVE_BAD];
			break;

		case DMI_OPCODE_GET_RX_DESC_FRAM:
			ReqBlock->Counter = Adapter->MediaMandatory[MM_RECEIVE_ERROR_ALIGNMENT];
			break;

		case DMI_OPCODE_GET_RX_DESC_OFLO:
			ReqBlock->Counter = Adapter->MediaOptional[MO_RECEIVE_OVERRUN];
			break;

		case DMI_OPCODE_GET_RX_DESC_CRC:
			ReqBlock->Counter = Adapter->GeneralOptional[GO_RECEIVE_CRC-GO_ARRAY_START];
			break;

		case DMI_OPCODE_GET_RX_DESC_BUF:
			ReqBlock->Counter = Adapter->DmiSpecific[DMI_RXBUFF_ERR];
			break;

		case DMI_OPCODE_GET_LANGUAGE:
			ReqBlock->Value = MY_LANGUAGE;
			break;

		case DMI_OPCODE_GET_STATUS:
			ReqBlock->addr[REDUNDANT_STATE] = (UCHAR)0;
			ReqBlock->addr[LINK_STATE] = (UCHAR) Adapter->CableDisconnected;
			break;

		case DMI_OPCODE_GET_NET_TOP:
#if 0
			if (Adapter->DeviceType == PCNET_PCI3)
				LanceGetActiveMediaInfo (Adapter);
#endif //0
			ReqBlock->Value = (Adapter->LineSpeed == 10)?TOPOL_10MB_ETH:TOPOL_100MB_ETH;
         break;

		case DMI_OPCODE_GET_RAM_SIZE:
			LANCE_READ_BCR (Adapter->MappedIoBaseAddress, 25, &Data);
			ReqBlock->Value = ((USHORT)Data << 8);
			break;

		default:	
			ReqBlock->Status = DMI_REQ_FAILURE;
			break;
	}
}
STATIC
USHORT
LancePortAccess (PLANCE_ADAPTER Adapter, ULONG * pData, USHORT Mode)
{

USHORT	RegType		= Mode & MASK_REGTYPE;
USHORT	AccessType	= Mode & MASK_ACCESS;
USHORT	RetCode		= LANCE_PORT_SUCCESS;

		switch (RegType)
		{
			case CSR_REG:
				switch (AccessType)
				{
					case PORT_READ:
						LANCE_READ_CSR (Adapter->MappedIoBaseAddress, Adapter->CsrNum, pData);
						break;

					case PORT_WRITE:
						LANCE_WRITE_CSR (Adapter->MappedIoBaseAddress, Adapter->CsrNum, *pData);
						break;

					default:
						RetCode = LANCE_PORT_FAILURE;
				}
				break;

			case BCR_REG:
				switch (AccessType)
				{
					case PORT_READ:
						LANCE_READ_BCR (Adapter->MappedIoBaseAddress, Adapter->BcrNum, pData);
						break;

					case PORT_WRITE:
						LANCE_WRITE_BCR (Adapter->MappedIoBaseAddress, Adapter->BcrNum, *pData);
						break;

					default:
						RetCode = LANCE_PORT_FAILURE;
				}
				break;

			default:
				RetCode = LANCE_PORT_FAILURE;

		}

	return RetCode;
}
#endif //DMI


#ifdef NDIS50_PLUS

ULONG
LanceTranslatePattern(
	IN PPMR pPMR,
//	IN PLANCE_ADAPTER Adapter,
   IN PVOID InformationBuffer,
   IN ULONG InformationBufferLength
	)
/*
Description :
	Translate the given pattern and pattern mask into PCnet-Fast+ 
	PMR format.
Return :	Size of the used pPMR->pmDataByes
			0 : INVALID 
*/				
{
	PUCHAR	PatternMask;
	PUCHAR	Pattern, DbgPattern;
	ULONG		MaskSize;
	ULONG		PatternSize;
	ULONG		TempPatternSize;
	ULONG		PatternOffset;
	ULONG		Skip;					// number of DWORD to Skip (max 7!!!)
	ULONG		currentSize;		// current size copied into *pPMR
	ULONG		i;
	UCHAR		byteData, tmpData;
//	PPMR		pPMR = &Adapter->tempStorage;

	currentSize = Skip = 0;

	MaskSize = ((PNDIS_PM_PACKET_PATTERN)InformationBuffer)->MaskSize;
	PatternSize = ((PNDIS_PM_PACKET_PATTERN)InformationBuffer)->PatternSize;
	PatternOffset = ((PNDIS_PM_PACKET_PATTERN)InformationBuffer)->PatternOffset;
	TempPatternSize = PatternSize;	

	if (MaskSize != PatternSize/8 + (PatternSize%8 ? 1 : 0))
	{
		#ifdef DBG
		if (LancePMDbg)
		{
			DbgPrint("MaskSize * 8 != PatternSize\n");
			DbgPrint("MaskSize %i, PatternSize %i\n", MaskSize,PatternSize);
		}
		#endif
		return 0;
	}
	PatternMask = (PUCHAR)InformationBuffer + sizeof(NDIS_PM_PACKET_PATTERN);
	Pattern = (PUCHAR)InformationBuffer + PatternOffset;

	#ifdef DBG
		if (LancePMDbg)
		{
			DbgPrint("Info %i, PatternSize %i, PatternOffset %i\n",
						InformationBufferLength,PatternSize,PatternOffset);
		}
	#endif

	if (InformationBufferLength < PatternSize + PatternOffset)
	{
		#ifdef DBG
		if (LancePMDbg)
		{
			DbgPrint("InfoBufferLength < PatternSize + PatternOffset\n");
			DbgPrint("Info %i, PatternSize %i, PatternOffset %i\n",
						InformationBufferLength,PatternSize,PatternOffset);
		}
		#endif
		return 0;
	}

	#ifdef DBG
	if (LancePMDbg)
	{
		DbgPrint("Mask :");
		DbgPattern = (PUCHAR)InformationBuffer + sizeof(NDIS_PM_PACKET_PATTERN);
		for (i=0; i<MaskSize; i++)
		{
			if (i%32 == 0)
				DbgPrint("\n");
			DbgPrint("%x ", *DbgPattern);
			DbgPattern++;
		}
		DbgPrint("\n");

		DbgPrint("Pattern :");
		DbgPattern = (PUCHAR)InformationBuffer + PatternOffset;
		for (i=0; i<PatternSize; i++)
		{
			if (i%32 == 0)
				DbgPrint("\n");
			DbgPrint("%x ", *DbgPattern);
			DbgPattern++;
		}
		DbgPrint("\n");
	}
	#endif

	#ifdef DBG
	if (LancePMDbg)
		DbgPrint("sizeof pPMR %i\n",sizeof(*pPMR));
		if(sizeof(*pPMR) != sizeof(PMR))
			DbgPrint("BugBug !!!, PMR size is not correct\n");
	#endif

	for (i=0; i<MaskSize; i++)
	{
		byteData = *PatternMask;
		PatternMask++;
		
		// check Byte0, Byte1, Byte2, Byte3 if they are not masked out
		// if so
		//	   1. save all 4 Bytes into pPMR->pmDataBytes[cS]
		//		2. save Skip and PatternMask into pPMR->PatternControl[cS]
		//		3.	increase currentSize(cS)
		// otherwise
		//  	1. increase Skip and if Skip = 8, then do above 3 steps and
		//			set Skip to 0.
		if (byteData & 0x0F)
		{
			if (TempPatternSize < 4)
			{
				switch (TempPatternSize)
				{
					case 1:
							pPMR->pmDataBytes[currentSize].DataByte0 = *Pattern;
							pPMR->pmDataBytes[currentSize].DataByte1 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte2 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte3 = 0x00;
							break;
					case 2:
							pPMR->pmDataBytes[currentSize].DataByte0 = *Pattern;
							pPMR->pmDataBytes[currentSize].DataByte1 = *(Pattern+1);
							pPMR->pmDataBytes[currentSize].DataByte2 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte3 = 0x00;
							break;
					case 3:
							pPMR->pmDataBytes[currentSize].DataByte0 = *Pattern;
							pPMR->pmDataBytes[currentSize].DataByte1 = *(Pattern+1);
							pPMR->pmDataBytes[currentSize].DataByte2 = *(Pattern+2);
							pPMR->pmDataBytes[currentSize].DataByte3 = 0x00;
							break;
					default:
							pPMR->pmDataBytes[currentSize].DataByte0 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte1 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte2 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte3 = 0x00;
							break;
				}

				pPMR->PatternControl[currentSize] = (USHORT)((byteData & 0x0F) | (Skip<<4)) << 8;
				Skip = 0;
				currentSize++;
				break;		
			}

			pPMR->pmDataBytes[currentSize].DataByte0 = *Pattern;
			pPMR->pmDataBytes[currentSize].DataByte1 = *(Pattern+1);
			pPMR->pmDataBytes[currentSize].DataByte2 = *(Pattern+2);
			pPMR->pmDataBytes[currentSize].DataByte3 = *(Pattern+3);

			pPMR->PatternControl[currentSize] = (USHORT)((byteData & 0x0F) | (Skip<<4)) << 8;
			Skip = 0;
			currentSize++;
		}
		else
		{
			if (Skip == 7)
			{
				if (TempPatternSize < 4)
				{
					switch (TempPatternSize)
					{
						case 1:
							pPMR->pmDataBytes[currentSize].DataByte0 = *Pattern;
							pPMR->pmDataBytes[currentSize].DataByte1 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte2 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte3 = 0x00;
							break;
						case 2:
							pPMR->pmDataBytes[currentSize].DataByte0 = *Pattern;
							pPMR->pmDataBytes[currentSize].DataByte1 = *(Pattern+1);
							pPMR->pmDataBytes[currentSize].DataByte2 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte3 = 0x00;
							break;
						case 3:
							pPMR->pmDataBytes[currentSize].DataByte0 = *Pattern;
							pPMR->pmDataBytes[currentSize].DataByte1 = *(Pattern+1);
							pPMR->pmDataBytes[currentSize].DataByte2 = *(Pattern+2);
							pPMR->pmDataBytes[currentSize].DataByte3 = 0x00;
							break;
						default:
							pPMR->pmDataBytes[currentSize].DataByte0 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte1 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte2 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte3 = 0x00;
							break;
					}

					pPMR->PatternControl[currentSize] = (USHORT)((byteData & 0x0F)|(Skip << 4)) << 8;
					Skip = 0;
					currentSize++;
					break;		
				}

				pPMR->pmDataBytes[currentSize].DataByte0 = *Pattern;
				pPMR->pmDataBytes[currentSize].DataByte1 = *(Pattern+1);
				pPMR->pmDataBytes[currentSize].DataByte2 = *(Pattern+2);
				pPMR->pmDataBytes[currentSize].DataByte3 = *(Pattern+3);
				pPMR->PatternControl[currentSize] = (USHORT)((byteData & 0x0F)|(Skip << 4)) << 8;
				currentSize++;
				Skip = 0;
			}
			else Skip++;
		}
		Pattern += 4;
		TempPatternSize -= 4;

		// check Byte4, Byte5, Byte6, Byte7 if they are not masked out
		// and do the above step
		if (byteData & 0xF0)
		{
			if (TempPatternSize < 4)
			{
				switch (TempPatternSize)
				{
					case 1:
							pPMR->pmDataBytes[currentSize].DataByte0 = *Pattern;
							pPMR->pmDataBytes[currentSize].DataByte1 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte2 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte3 = 0x00;
							break;
					case 2:
							pPMR->pmDataBytes[currentSize].DataByte0 = *Pattern;
							pPMR->pmDataBytes[currentSize].DataByte1 = *(Pattern+1);
							pPMR->pmDataBytes[currentSize].DataByte2 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte3 = 0x00;
							break;
					case 3:
							pPMR->pmDataBytes[currentSize].DataByte0 = *Pattern;
							pPMR->pmDataBytes[currentSize].DataByte1 = *(Pattern+1);
							pPMR->pmDataBytes[currentSize].DataByte2 = *(Pattern+2);
							pPMR->pmDataBytes[currentSize].DataByte3 = 0x00;
							break;
					default:
							pPMR->pmDataBytes[currentSize].DataByte0 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte1 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte2 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte3 = 0x00;
							break;
				}

				pPMR->PatternControl[currentSize] = (USHORT)((byteData >> 4 & 0x0F) | (Skip << 4)) << 8;
				Skip = 0;
				currentSize++;
				break;		
			}

			pPMR->pmDataBytes[currentSize].DataByte0 = *Pattern;
			pPMR->pmDataBytes[currentSize].DataByte1 = *(Pattern+1);
			pPMR->pmDataBytes[currentSize].DataByte2 = *(Pattern+2);
			pPMR->pmDataBytes[currentSize].DataByte3 = *(Pattern+3);
			pPMR->PatternControl[currentSize] = (USHORT)((byteData >> 4 & 0x0F) | (Skip << 4)) << 8;
			Skip = 0;
			currentSize++;
		}
		else
		{
			if (Skip == 7)
			{
				if (TempPatternSize < 4)
				{
					switch (TempPatternSize)
					{
						case 1:
							pPMR->pmDataBytes[currentSize].DataByte0 = *Pattern;
							pPMR->pmDataBytes[currentSize].DataByte1 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte2 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte3 = 0x00;
							break;
						case 2:
							pPMR->pmDataBytes[currentSize].DataByte0 = *Pattern;
							pPMR->pmDataBytes[currentSize].DataByte1 = *(Pattern+1);
							pPMR->pmDataBytes[currentSize].DataByte2 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte3 = 0x00;
							break;
						case 3:
							pPMR->pmDataBytes[currentSize].DataByte0 = *Pattern;
							pPMR->pmDataBytes[currentSize].DataByte1 = *(Pattern+1);
							pPMR->pmDataBytes[currentSize].DataByte2 = *(Pattern+2);
							pPMR->pmDataBytes[currentSize].DataByte3 = 0x00;
							break;
						default:
							pPMR->pmDataBytes[currentSize].DataByte0 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte1 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte2 = 0x00;
							pPMR->pmDataBytes[currentSize].DataByte3 = 0x00;
							break;
					}

					pPMR->PatternControl[currentSize] = (USHORT)((byteData >> 4 & 0x0F)|(Skip << 4)) << 8;
					Skip = 0;
					currentSize++;
					break;		
				}

				pPMR->pmDataBytes[currentSize].DataByte0 = *Pattern;
				pPMR->pmDataBytes[currentSize].DataByte1 = *(Pattern+1);
				pPMR->pmDataBytes[currentSize].DataByte2 = *(Pattern+2);
				pPMR->pmDataBytes[currentSize].DataByte3 = *(Pattern+3);
				pPMR->PatternControl[currentSize] = (USHORT)((byteData >> 4 & 0x0F)|(Skip << 4)) << 8;
				currentSize++;
				Skip = 0;
			}
			else Skip++;
		}
		Pattern += 4;
		TempPatternSize -= 4;

	} /* for */
/*
	#ifdef DBG
	if (LancePMDbg)
		DbgPrint("PatternSize check.\n");
	#endif

	switch (PatternSize % 4)
	{
		case 1:
				pPMR->pmDataBytes[currentSize-1].DataByte1 = 0x00;
			
		case 2:
				pPMR->pmDataBytes[currentSize-1].DataByte2 = 0x00;

		case 3:
				pPMR->pmDataBytes[currentSize-1].DataByte3 = 0x00;
				break;
		default:
				break;
	}
	#ifdef DBG
	if (LancePMDbg)
		DbgPrint("PatternSize checked.\n");
	#endif
*/
	pPMR->PatternControl[currentSize-1] |= 0x8000; 
	
	#ifdef DBG
	if (LancePMDbg)
		DbgPrint("currentSize is %i.\n",currentSize);
	#endif
	return currentSize;
} /* LanceTranslatePattern */

PLIST_ENTRY
FindFirstFitBlock(
	IN PLANCE_ADAPTER Adapter,
	ULONG	usedSize
	)
/*
	Travel FreeBlock list and find a block which is big enough to hold the 
	given Pattern.
*/
{
	ULONG nFreeBlocks;
	PLIST_ENTRY linkage;
	PPM_BLOCK pmBlock;

	nFreeBlocks = Adapter->pmManager.freeBlocks;
	linkage = NULL;
	pmBlock = (PPM_BLOCK)Adapter->pmManager.freeList.Flink;

	if (!nFreeBlocks)
		return linkage;

	while (nFreeBlocks--)
	{
		if (pmBlock	== NULL)
			break;

		if (pmBlock->blockSize >= usedSize)
		{
			linkage = &(pmBlock->linkage);	
//			Adapter->pmManager.freeBlocks--;
			break;
		}
		else
			pmBlock = (PPM_BLOCK)pmBlock->linkage.Flink;
	}

	return linkage;
}

PLIST_ENTRY
FindPattern(
	IN PLANCE_ADAPTER Adapter,
	IN PPMR pmr,
	IN ULONG usedSize
	)
{
	ULONG nPatternBlocks;
	PPM_BLOCK pmBlock;
	PPMR	adapterPMR;
	ULONG wordAdapterPMR;
	ULONG wordTmpPMR;
	ULONG i;
	USHORT adapterPatternControl;
	USHORT tmpPatternControl;
	BOOLEAN PatternDismatch;

	nPatternBlocks = Adapter->pmManager.patterns;
	pmBlock = (PPM_BLOCK)Adapter->pmManager.patternList.Flink;
	adapterPMR = &Adapter->pmManager.pmRAM;
	PatternDismatch = FALSE;

	if (!nPatternBlocks)
	{

		#if DBG
			if (LancePMDbg)
				DbgPrint("No Patterns in PMR\n");
		#endif

		return NULL;
	}

	#if DBG
		if (LancePMDbg)
			DbgPrint("No. of Patterns in PMR is %d\n",nPatternBlocks);
	#endif

	while (nPatternBlocks--)
	{
		if (pmBlock	== NULL)
			break;

		if (pmBlock->blockSize != usedSize)
		{
			#if DBG
				if (LancePMDbg)
				DbgPrint("Two Blocks need to be compared has different size, %d, %d\n",
							pmBlock->blockSize,usedSize);
			#endif

			pmBlock = (PPM_BLOCK)pmBlock->linkage.Flink;
			continue;
		}

		PatternDismatch = FALSE;

		for (i = 0; i < usedSize; i++)
		{
			wordAdapterPMR = *((PULONG)(&(adapterPMR->pmDataBytes[pmBlock->startAddress+i].DataByte0)));
			wordTmpPMR = *((PULONG)(&(pmr->pmDataBytes[i].DataByte0)));
		
			if (wordAdapterPMR != wordTmpPMR)
			{
				#if DBG
					if (LancePMDbg)
						DbgPrint("Pattern Dismatch : %x : %x , %dth address\n",
									wordAdapterPMR,wordTmpPMR,i);
				#endif

				PatternDismatch = TRUE;
				break;
			}

			adapterPatternControl = adapterPMR->PatternControl[pmBlock->startAddress+i];
			tmpPatternControl = pmr->PatternControl[i];

			if (adapterPatternControl != tmpPatternControl)
			{
				#if DBG
					if (LancePMDbg)
						DbgPrint("Control Dismatch : %x : %x , %dth address\n",
									adapterPatternControl,tmpPatternControl,i);
				#endif
				PatternDismatch = TRUE;
				break;
			}

			
		}
		
		if (PatternDismatch)
		{
			pmBlock = (PPM_BLOCK)(pmBlock->linkage.Flink);
			continue;
		}
		
		return &(pmBlock->linkage);
	}

	return NULL;
}

NDIS_STATUS
LanceAddWakeUpPattern(
	PLANCE_ADAPTER Adapter,
   IN PVOID InformationBuffer,
   IN ULONG InformationBufferLength
	)
{
	PMR	pmr;				// tmp storage to copy Pattern
	ULONG	usedSize;		// used tmp storage size
	PLIST_ENTRY linkage;
	PPM_BLOCK	pFreeBlock;
	PPM_BLOCK	pPmBlock;
	NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
	PCHAR	addressByte;
	USHORT i;
	
	usedSize = LanceTranslatePattern(&pmr, InformationBuffer, InformationBufferLength);

	#if DBG
		if (LancePMDbg)
		DbgPrint("Used PMR size : %d\n", usedSize);
	#endif

	if (!usedSize)
		return NDIS_STATUS_INVALID_LENGTH;


	/*
		if (patterns >= MAX_ALLOWED_PATTERN) 
			then return NOT_ENOUGH_SPACE
		travel FreeBlock List to find free block in the PMR
>>>>>>if found free block with size >= usedSize, then do the following,
	 		1. if block size > usedSize,
					1.1 get a pmBlock from pool.
					1.2 update current free block (start address and size)
					1.3 copy pmr into the new pmBlock
					1.4 insert the pmBlock into the patternList
					1.5 update number of patterns and totalPMRUsed
					1.6 update Address space in PMR
			2. else (block size == usedSize)
					2.1 remove the block from free block list
					2.2 update number of free blocks
					2.3 copy pmr into the pmBlock
					2.4 insert the block into the patternList
					2.5 update number of patterns and totalPMRUsed
					2.6 update Address space in PMR
		else (not able to find one)
			1. check if totalPMRUsed + usedSize <= MAX_PMR_ADDRESS - BASE_PM_START_ADDRESS
				if so
					1.1 run Defragmentation
					1.2 go to >>>>>>>
				else
					return NOT_ENOUGH_SPACE
	*/

	if (Adapter->pmManager.patterns >= MAX_ALLOWED_PATTERN)
		return NDIS_STATUS_NOT_ACCEPTED;	// Already 8 patterns are filled.

	linkage = FindFirstFitBlock(Adapter,usedSize);
	if (linkage == NULL)
	{
		#if DBG
			if (LancePMDbg)
			DbgPrint("Not able to find a free block with size %x.\n",usedSize);
		#endif

		if (Adapter->pmManager.totalPMRUsed+usedSize <= MAX_PMR_ADDRESS - BASE_PM_START_ADDRESS)
		{	DefragmentPMR(Adapter);
			linkage = FindFirstFitBlock(Adapter,usedSize);
			if (linkage == NULL)
			{
				#if DBG
					if (LancePMDbg)
					DbgPrint("Serious Bug!!! Modify the codes.\n");
				#endif
				return NDIS_STATUS_NOT_ACCEPTED;	//NOT_ENOUGH_SPACE;
			}
		}
		else
			return NDIS_STATUS_NOT_ACCEPTED;	//NOT_ENOUGH_SPACE;
	}

	pFreeBlock = (PPM_BLOCK)linkage;

	if (pFreeBlock->blockSize > usedSize)
	{
		// 1.1 get one block from pmBlockPool
		pPmBlock = (PPM_BLOCK)RemoveHeadList(&(Adapter->pmManager.pmBlockPool));
		pPmBlock->startAddress = pFreeBlock->startAddress;
		pPmBlock->blockSize = usedSize;	
		// 1.2 update current free block
		pFreeBlock->startAddress += usedSize;
		pFreeBlock->blockSize -= usedSize;	
	}
	else
	{
		// 2.1 remove the block from free block list
		RemoveEntryList(&pFreeBlock->linkage);
		// 2.2 update number of free blocks
		Adapter->pmManager.freeBlocks--;
		pPmBlock = pFreeBlock;
	}

	// 1.3 & 2.3 copy pmr into the pmBlock
	NdisMoveMemory(&(Adapter->pmManager.pmRAM.pmDataBytes[pPmBlock->startAddress].DataByte0),
						pmr.pmDataBytes,
//						Adapter->tempStorage.pmDataBytes,
						usedSize * sizeof(ULONG));

	NdisMoveMemory(&(Adapter->pmManager.pmRAM.PatternControl[pPmBlock->startAddress]),
						pmr.PatternControl,
//						Adapter->tempStorage.PatternControl,
						usedSize * sizeof(USHORT));
	
	// 1.4 & 2.4 insert the pmBlock into the patternList
	InsertTailList(&(Adapter->pmManager.patternList), &pPmBlock->linkage);

	// 1.5 & 2.5 update number of patterns and totalPMRUsed
	Adapter->pmManager.totalPMRUsed += usedSize;
	Adapter->pmManager.patterns++;

	// 1.6 & 2.6 update Address space in pmRAM
	addressByte = &Adapter->pmManager.pmRAM.pmDataBytes[0].DataByte0;
	for (i=0; i<MAX_ALLOWED_PATTERN; i++)
	{
		if (!(Adapter->pmManager.pmRAM.PatternControl[0] & (0x0100 << i)))
		{
			Adapter->pmManager.pmRAM.PatternControl[0] |= 0x0100 << i;
			break;
		}
	}

	#if DBG
	if (LancePMDbg && (i >= MAX_ALLOWED_PATTERN))
		DbgPrint("All address MASK field are set to 1. Bug!!!\n");
	#endif
	
	addressByte += i;
	*addressByte = (UCHAR)pPmBlock->startAddress;

/*
	// update pattern address pointer
	for (i=0; i<MAX_ALLOWED_PATTERN; i++)
	{
		if (!(Adapter->pmManager.pmRAM.PatternControl[0] & (0x0100 << i)))
		{
			Adapter->pmManager.pmRAM.PatternControl[0] |= 0x0100 << i;
			
			switch (i) {
				case 0 :
					Adapter->pmManager.pmRAM.pmDataBytes[0].DataByte0 = (UCHAR)pPmBlock->startAddress;
					break;
				case 1 :
					Adapter->pmManager.pmRAM.pmDataBytes[0].DataByte1 = (UCHAR)pPmBlock->startAddress;
					break;
				case 2 :
					Adapter->pmManager.pmRAM.pmDataBytes[0].DataByte2 = (UCHAR)pPmBlock->startAddress;
					break;
				case 3 :
					Adapter->pmManager.pmRAM.pmDataBytes[0].DataByte3 = (UCHAR)pPmBlock->startAddress;
					break;
				case 4 :
					Adapter->pmManager.pmRAM.pmDataBytes[1].DataByte0 = (UCHAR)pPmBlock->startAddress;
					break;
				case 5 :
					Adapter->pmManager.pmRAM.pmDataBytes[1].DataByte1 = (UCHAR)pPmBlock->startAddress;
					break;
				case 6 :
					Adapter->pmManager.pmRAM.pmDataBytes[1].DataByte2 = (UCHAR)pPmBlock->startAddress;
					break;
				case 7 :
					Adapter->pmManager.pmRAM.pmDataBytes[1].DataByte3 = (UCHAR)pPmBlock->startAddress;
					break;

			}

			break;
		}
	}
*/
	return Status;
}

NDIS_STATUS
LanceRemoveWakeUpPattern(
	PLANCE_ADAPTER Adapter,
   IN PVOID InformationBuffer,
   IN ULONG InformationBufferLength
	)
{
	PMR	pmr;				// tmp storage to copy Pattern
	PLIST_ENTRY linkage;
	PPM_BLOCK	pPatternBlock;
	PPM_BLOCK	pFreeBlock;
	PPM_DATABYTES pPmDataBytes;
	NDIS_STATUS Status;
	ULONG		AddressWord;
	ULONG		usedSize;
	PUCHAR 	addressByte;
	USHORT	i;
	USHORT	j;
	BOOLEAN	MoveUPOn;
	UCHAR		address;

	Status = NDIS_STATUS_SUCCESS;
	pPmDataBytes = Adapter->pmManager.pmRAM.pmDataBytes;
	MoveUPOn = FALSE;
	usedSize = LanceTranslatePattern(&pmr, InformationBuffer, InformationBufferLength);
//	usedSize = LanceTranslatePattern(Adapter, InformationBuffer, InformationBufferLength);

	if (!usedSize)
		return NDIS_STATUS_INVALID_LENGTH;

	#if DBG
		if (LancePMDbg)
		DbgPrint("Used PMR size : %d\n", usedSize);
	#endif

	/* find Pattern in the patternList */
	linkage = FindPattern(Adapter,&pmr,usedSize);
//	linkage = FindPattern(Adapter,&Adapter->tempStorage,usedSize);

	if (linkage == NULL)
		return NDIS_STATUS_NOT_ACCEPTED; //find correct status

	pPatternBlock = (PPM_BLOCK)linkage;
	address = (UCHAR)pPatternBlock->startAddress;

	// disable pattern address pointer
	addressByte = &Adapter->pmManager.pmRAM.pmDataBytes[0].DataByte0;
	for (i=0; i<MAX_ALLOWED_PATTERN; i++)
	{
		if ((UCHAR)pPatternBlock->startAddress == *(addressByte + i))
		{
			Adapter->pmManager.pmRAM.PatternControl[0] &= ~(0x0100<<i);
			*(addressByte+i) = 0x00;
			break;
		}
	}

	#if DBG
	if (LancePMDbg && (i >= MAX_ALLOWED_PATTERN))
		DbgPrint("Address field mismatch. Bug!!!\n");
	#endif

/*
	for (i=0; i<MAX_ALLOWED_PATTERN; i++)
	{
		switch (i) {
			case 0 :
				addressByte = pPmDataBytes->DataByte0;
				break;
			case 1 :
				addressByte = pPmDataBytes->DataByte1;
				break;
			case 2 :
				addressByte = pPmDataBytes->DataByte2;
				break;
			case 3 :
				addressByte = pPmDataBytes->DataByte3;
				break;
			case 4 :
				addressByte = (pPmDataBytes+1)->DataByte0;
				break;
			case 5 :
				addressByte = (pPmDataBytes+1)->DataByte1;
				break;
			case 6 :
				addressByte = (pPmDataBytes+1)->DataByte2;
				break;
			case 7 :
				addressByte = (pPmDataBytes+1)->DataByte3;
				break;

		}

		if (address == (USHORT)addressByte)
		{
			Adapter->pmManager.pmRAM.PatternControl[0] &= ~(0x0100<<i);
			break;
		}
	}
*/
	RemoveEntryList(linkage);
	Adapter->pmManager.patterns--;
	Adapter->pmManager.totalPMRUsed -= usedSize;
	Adapter->pmManager.freeBlocks++;
	InsertTailList(&(Adapter->pmManager.freeList),linkage);

	return Status;	
}

VOID
DefragmentPMR(
	PLANCE_ADAPTER Adapter
	)
/*
	1. Travel each pmBlock in the patternList.
		1.1 Modify start Address of pmBlock
		1.2 Modify tmpPMR (Address field, Data field, and Control filed)
	2. Copy tmpPMR into Adapter PMR.
	3. Keep just one freeBlock in the freeBlockList and 
		update startAddress and blockSize.
*/
{
	PMR pmr;
	ULONG nPatternBlocks;
	PPM_BLOCK pmBlock;
	PPMR	adapterPMR;
	ULONG i;
	ULONG	startAddress;
	PUCHAR addressByte;
	PLIST_ENTRY linkage;

	pmBlock = (PPM_BLOCK)Adapter->pmManager.patternList.Flink;
	adapterPMR = &Adapter->pmManager.pmRAM;
	startAddress = BASE_PM_START_ADDRESS;
	addressByte = &pmr.pmDataBytes[0].DataByte0;

	NdisZeroMemory(&pmr, sizeof(PMR));

	nPatternBlocks = Adapter->pmManager.patterns;
	for (i=0; i<nPatternBlocks; i++)
	{
		pmBlock->startAddress = startAddress;

		pmr.PatternControl[0] |= 0x0100 << i;
		*(addressByte+i) = (UCHAR)pmBlock->startAddress;

		NdisMoveMemory(&pmr.pmDataBytes[startAddress].DataByte0,
							&(Adapter->pmManager.pmRAM.pmDataBytes[pmBlock->startAddress].DataByte0),
							pmBlock->blockSize * sizeof(ULONG));

		NdisMoveMemory(&pmr.PatternControl[startAddress],
							&(Adapter->pmManager.pmRAM.PatternControl[pmBlock->startAddress]),
							pmBlock->blockSize * sizeof(USHORT));
	
		startAddress += pmBlock->blockSize;
		pmBlock = (PPM_BLOCK)pmBlock->linkage.Flink;
	}
			
	nPatternBlocks = Adapter->pmManager.freeBlocks;
	while (nPatternBlocks-- > 1)
	{
		// remove a linkage from freeList and reinitialize the linkage
		linkage = RemoveHeadList(&(Adapter->pmManager.freeList));
		pmBlock = (PPM_BLOCK)linkage;
		pmBlock->startAddress = 0;
		pmBlock->blockSize = 0;
		// insert the linkage into the pmBlockPool
		InsertTailList(&(Adapter->pmManager.pmBlockPool), linkage);
		Adapter->pmManager.freeBlocks--;
	}

	pmBlock = (PPM_BLOCK)Adapter->pmManager.freeList.Flink;
	pmBlock->startAddress = startAddress;
	pmBlock->blockSize = MAX_PMR_ADDRESS - startAddress;

	NdisMoveMemory(&(Adapter->pmManager.pmRAM.pmDataBytes[0].DataByte0),
						&pmr.pmDataBytes[0].DataByte0,
						MAX_PMR_ADDRESS * sizeof(ULONG));

	NdisMoveMemory(&(Adapter->pmManager.pmRAM.PatternControl[0]),
						&pmr.PatternControl[0],
						MAX_PMR_ADDRESS * sizeof(USHORT));
	
}

NDIS_STATUS
LanceEnableWakeUp(
	PLANCE_ADAPTER Adapter,
   IN PVOID InformationBuffer,
   IN ULONG InformationBufferLength
	)
/*
   InformationBuffer - Holds the data to be set.
   InformationBufferLength - The length of InformationBuffer.
									  This length should be sizeof(ULONG).
*/
{
	if (InformationBufferLength != sizeof(ULONG))
		return NDIS_STATUS_INVALID_LENGTH;
/*
	switch (*(PULONG)InformationBuffer)
	{
		case NDIS_PNP_WAKE_UP_MAGIC_PACKET:
				Adapter->WakeUpMode |= LANCE_WAKE_UP_MAGIC_PACKET;
				break;
		case NDIS_PNP_WAKE_UP_PATTERN_MATCH:
				Adapter->WakeUpMode |= LANCE_WAKE_UP_PATTERN_MATCH;
				break;
		case NDIS_PNP_WAKE_UP_LINK_CHANGE:
				Adapter->WakeUpMode |= LANCE_WAKE_UP_LINK_CHANGE;
				break;
	}
*/
	Adapter->WakeUpMode = *(PULONG)InformationBuffer;

// MJ : Testing PM Mode, Enable all three modes,
//	if (Adapter->ActivePhyAddress != HOMERUN_PHY_ADDRESS)		
		Adapter->WakeUpMode = 0x07;
//	else
//		Adapter->WakeUpMode = 0x03; // HomeRun doesn't support Link Status yet.

	return NDIS_STATUS_SUCCESS;
}
		

VOID
LanceEnableMagicPacket(
	PLANCE_ADAPTER Adapter
	)
{
	USHORT Data;

	/*
		Adapter is already suspended/interrupt-disabled.
	*/
//	LanceStopChip(Adapter);

	/*
		set the chip to Suspend mode
	*/
//	LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR5, &Data);
//	Data &= 0xFFFE;
//	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR5, Data);

	/*
		Set MPMODE bit, MPEN bit, and MPPLBA bit.
	*/
	LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR5, &Data);
	Data |= 0x0026;
	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR5, Data);

	/*
		Enable MPPEN in CSR 116. Don't enable PME_EN_OVR.
		Magic Packet won't work with PME_EN_OVER bit set.
	*/

	LANCE_READ_CSR(Adapter->MappedIoBaseAddress, 116, &Data);
	Data |= 0x0050; 
	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, 116, Data);

}

VOID
LanceEnablePatternMatch(
	PLANCE_ADAPTER Adapter
	)
{
	PPMR pmr = &Adapter->pmManager.pmRAM;
	USHORT Data;
	USHORT i;
	ULONG PatchData = 0;

	/*
		Adapter is already stoped/suspended/interrupt-disabled.
	*/

	/*
		PMAT_MODE bit in BCR45 set to 0.
	*/
//	LANCE_READ_BCR(Adapter->MappedIoBaseAddress, 45, &Data);
//	Data &= 0xFF00;
	Data = 0;
	LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, 45, Data);

	for (i=0; i<MAX_PMR_ADDRESS; i++)
	{
		Data = pmr->PatternControl[i];
		Data |= i;
		LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, 45, Data);

		#if DBG
			if (LancePMDbg)
			DbgPrint("BCR45 : %x, ", Data);
		#endif

		Data = (USHORT)(pmr->pmDataBytes[i].DataByte1 << 8 |
							 pmr->pmDataBytes[i].DataByte0);
		LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, 46, Data);

		#if DBG
			if (LancePMDbg)
			DbgPrint("BCR46 : %x, ", Data);
		#endif

		Data = (USHORT)(pmr->pmDataBytes[i].DataByte3 << 8 |
							 pmr->pmDataBytes[i].DataByte2);
		LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, 47, Data);

		#if DBG
			if (LancePMDbg)
			DbgPrint("BCR47 : %x\n", Data);
		#endif
	}

//	LANCE_READ_BCR(Adapter->MappedIoBaseAddress, 45, &Data);
	Data = 0x0080;
	LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, 45, Data);

	//
	// Write a zero to a read only register (offset 0 in PCI config
	// space) for bug fixing
	//
	WRITE_PCI_ID_ZERO(Adapter,&PatchData)

	//
	// Enable PME_EN
	//
	if (NdisReadPciSlotInformation(
				Adapter->LanceMiniportHandle,
				Adapter->LanceSlotNumber,
				PCI_PMCSR_REG,
				(PVOID)&Data,
				sizeof(Data)) == sizeof(Data))
	{
			#ifdef DBG
				if (LancePMDbg)
				{
					DbgPrint("PCI_PMCSR_REG is %x\n",Data);
				}
			#endif
		//
		// Check PME_EN bit
		//
		if (!(Data & PME_EN))
		{
			Data |= PME_EN;
			NdisWritePciSlotInformation(
					Adapter->LanceMiniportHandle,
					Adapter->LanceSlotNumber,
					PCI_PMCSR_REG,
					(PVOID)&Data,
					sizeof(Data));
		}
	}

}

VOID
LanceEnableLinkChange(
	PLANCE_ADAPTER Adapter
	)
{
	USHORT Data;

	/*
		Adapter is already stoped/suspended/interrupt-disabled
	*/
//	LanceStopChip(Adapter);

	/*
		Enable LCMODE in CSR 116. Don't enable PME_EN_OVR!!!
		Magic Packet won't work with PME_EN_OVER bit set.
	*/
	LANCE_READ_CSR(Adapter->MappedIoBaseAddress, 116, &Data);
	Data |= 0x0100; //0x0500;
	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, 116, Data);

}

VOID
LanceClearWakeUpMode(
	PLANCE_ADAPTER Adapter
	)
{
	USHORT Data;

	/*
		Adapter is already stoped/suspended/interrupt-disabled
	*/
//	LanceStopChip(Adapter);

	/*
		Disable PME_EN_OVR, LCMODE, EMPPLBA, and MMPPEN in CSR 116
	*/
	LANCE_READ_CSR(Adapter->MappedIoBaseAddress, 116, &Data);
	Data &= ~0x03a0; //~0x06a0 or ~0x07f0;
	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, 116, Data);

	/*
		Disable PMMODE bit in BCR45
	*/
	Data = 0;
	LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, 45, Data);

}

VOID
LanceQueryPWRState(
	PLANCE_ADAPTER Adapter
	)
{
	USHORT Data;
	ULONG PatchData = 0;

	//
	// Write a zero to a read only register (offset 0 in PCI config
	// space) for bug fixing
	//
	WRITE_PCI_ID_ZERO(Adapter,&PatchData)

	//
	// Read PMCSR (bit 1 and 0 are for Power State)
	//
	if (NdisReadPciSlotInformation(
				Adapter->LanceMiniportHandle,
				Adapter->LanceSlotNumber,
				PCI_PMCSR_REG,
				(PVOID)&Data,
				sizeof(Data)) == sizeof(Data))
	{
		Adapter->powerState = (NDIS_DEVICE_POWER_STATE) (Data & 0x0003) + 1;
	}
	else
	{
		Adapter->powerState = 0;
	}
}

VOID
LanceSetPWRState(
	PLANCE_ADAPTER Adapter
	)
{
	ULONG PatchData = 0;
	NDIS_DEVICE_POWER_STATE pwrState;
	UCHAR Data;

	pwrState = Adapter->powerState;

	if ((pwrState == NdisDeviceStateUnspecified) ||
		 (pwrState == NdisDeviceStateMaximum))
	{
		return;
	}

	Data = (UCHAR) pwrState - 1;

	//
	// Write a zero to a read only register (offset 0 in PCI config
	// space) for bug fixing
	//
	WRITE_PCI_ID_ZERO(Adapter,&PatchData)

	//
	// Read PMCSR (bit 1 and 0 are for Power State)
	//
	NdisWritePciSlotInformation(
				Adapter->LanceMiniportHandle,
				Adapter->LanceSlotNumber,
				PCI_PMCSR_REG,
				(PVOID)&Data,
				sizeof(Data));

}

VOID
LancePMModeInitSetup(
	PLANCE_ADAPTER Adapter
	)
/*
	Do the necessary work to put the adapter in PM Mode.
	For example : Disable Interrupt.
*/
{
	USHORT Data;
	//
	// Clean up the send Queue.
	//
	while(Adapter->FirstTxQueue)
	{
		PNDIS_PACKET QueuePacket = Adapter->FirstTxQueue;

		Adapter->NumPacketsQueued--;
		DequeuePacket(Adapter->FirstTxQueue, Adapter->LastTxQueue);
//   	NdisReleaseSpinLock(&Adapter->TxQueueLock);

		NdisMSendComplete(
				Adapter->LanceMiniportHandle,
				QueuePacket,
				NDIS_STATUS_RESET_IN_PROGRESS);

//		NdisAcquireSpinLock(&Adapter->TxQueueLock);
	}
//	NdisReleaseSpinLock(&Adapter->TxQueueLock);
	
	/*
		Disable device interrupts.	Only IENA is affected by writing 0
	*/
	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, 0);

	/*
		set the chip to Suspend mode
	*/
//	LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR5, &Data);
//	Data &= 0xFFFE;
//	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR5, Data);

	/* stop the chip */
//	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress,LANCE_CSR0,LANCE_CSR0_STOP | 0xff00);
	LanceStopChip(Adapter);

	#if DBG
		if (LanceRegChkDbg)
		{
			UINT temp;
			for(temp=0; temp<126; temp++)
			{
				LANCE_READ_CSR(Adapter->MappedIoBaseAddress, temp, &Data);
				DbgPrint("CSR%i : %x\n", temp,Data);
			}
			for(temp=0; temp<50; temp++)
			{
				LANCE_READ_BCR(Adapter->MappedIoBaseAddress, temp, &Data);
				DbgPrint("BCR%i : %x\n", temp,Data);
			}
		}
	#endif

}

VOID
LancePMModeRecover(
	PLANCE_ADAPTER Adapter
	)
{
	USHORT Data;
	USHORT Tries = 5;
	USHORT delayAfterReset = 20;

	#if DBG
		if (LancePMDbg)
		DbgPrint("LancePMModeRecover, Adapter->OpFlags = %x\n", Adapter->OpFlags);
	#endif

		//
		// Do Phy Reset
		//
		//LanceResetPHY(Adapter);
	
		//
		// Do hardware reset
		//
		NdisRawReadPortUshort(Adapter->MappedIoBaseAddress + LANCE_RESET_PORT, &Data);

		//
		// Delay after reset
		//
		while (delayAfterReset--)
			NdisStallExecution(50);

		#if DBG
			if (LancePMDbg)
			{
			LANCE_READ_CSR(Adapter->MappedIoBaseAddress, 116, &Data);
			DbgPrint("CSR116 %x\n",Data);
			}
		#endif

//		LanceClearWakeUpMode(Adapter);
		LanceHomeLanPortScan(Adapter);

		if (Adapter->ActivePhyAddress == HOMERUN_PHY_ADDRESS)
		{
			//HRON(Adapter);
			InitHomeRunPort(Adapter);
			// Program IPG to 0x64xx (Errata #1 for PCnet_HL) 
			LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR125, &Data);
//			Data = (Data & IPGMASK)|LANCE_CSR125_HLIPG;
			Data = (Data & IPGMASK)|((USHORT)Adapter->IPG<<8);
			LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR125, Data);
			if (Adapter->RunBugFix)
				EverestBugFix(Adapter);
		}
		else
		{
			HomeLanExtPhyConfig(Adapter);
		}

	while (Tries--)
	{
		/* Disable interrupts and release any pending interrupt sources	*/
		LanceDisableInterrupt(Adapter);

		#if DBG
			if (LancePMDbg)
			{
			LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, &Data);
			DbgPrint("DisableInterrupt OK. CSR0 %x\n",Data);
			}
		#endif

		LanceStopChip(Adapter);
//		LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, 0xff04);

		#if DBG
			if (LancePMDbg)
			{
			LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, &Data);
			DbgPrint("LanceStopChip OK. CSR0 %x\n",Data);
			}
		#endif

		/* Initialize registers and data structure	*/
		LanceSetupRegistersAndInit(Adapter, TRUE);

		#if DBG
			if (LancePMDbg)
			{
			LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, &Data);
			DbgPrint("LanceSetupReg OK. CSR0 %x\n",Data);
			LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR15, &Data);
			DbgPrint("LanceSetupReg OK. CSR15 %x\n",Data);
			if (Data & 0x0002)
				LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR15, Data&(~0x0002));
				
			}
		#endif

	#if DBG
		if (LanceRegChkDbg)
		{
			UINT temp;
			for(temp=0; temp<126; temp++)
			{
				LANCE_READ_CSR(Adapter->MappedIoBaseAddress, temp, &Data);
				DbgPrint("CSR%i : %x\n", temp,Data);
			}
			for(temp=0; temp<50; temp++)
			{
				LANCE_READ_BCR(Adapter->MappedIoBaseAddress, temp, &Data);
				DbgPrint("BCR%i : %x\n", temp,Data);
			}
		}
	#endif
		
		/* Start the chip	*/
		LanceStartChip(Adapter);

		LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, &Data);
		LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, &Data);

		if ((Data & LANCE_CSR0_RUNNING) != LANCE_CSR0_RUNNING)
		{
		#if DBG
			if (LancePMDbg)
			{
			DbgPrint("<==PMModeRecover failed. CSR0 %x\n",Data);
			LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, 0xff04);
			LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR15, &Data);
			DbgPrint("LanceSetupReg OK. CSR15 %x\n",Data);
			}
		#endif

		}
		else
		{
			return;		
		#if DBG
			if (LancePMDbg)
			{
			DbgPrint("<==PMModeRecover success. CSR0 %x\n",Data);
			}
		#endif
		}

	}

}

VOID
HRON(
	PLANCE_ADAPTER Adapter
	)
{
	USHORT Data;

	LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR49, 0x8202);
	LANCE_READ_MII(Adapter->MappedIoBaseAddress,
						(Adapter->ActivePhyAddress | ANR0), &Data);
	#ifdef DBG
		if(LanceHLDbg)
			DbgPrint("ANR0 %x\n",Data);
	#endif
	LANCE_WRITE_MII(Adapter->MappedIoBaseAddress,
						(Adapter->ActivePhyAddress | ANR0), Data&(~ANR0_ISO));
	LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR33, HOMERUN_PHY_ADDRESS|0x8000);
	LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR49, 0x8101);

//	NdisRawReadPortUshort(Adapter->MappedIoBaseAddress + LANCE_RESET_PORT, &TempData);
//	NdisStallExecution(500);
}		


#endif //NDIS50_PLUS
