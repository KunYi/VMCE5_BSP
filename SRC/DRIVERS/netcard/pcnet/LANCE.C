/*++

Copyright (c) 1998 ADVANCED MICRO DEVICES, INC. All Rights Reserved.
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

	lance.c

Abstract:

	This is the main file for the Advanced Micro Devices 
	HomeLan controller NDIS Device Driver.

Environment:
	Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:
 *
 * Rev 1.02.000 MJ. Link Pulse Off for the unselected PHYs.
 *					 SFEX IADJ register(28) set to 0x0500 (1010)
 * 
 * Rev 1.00.000 MJ. The very first version has been created based on
 *					 PCnet Family Device Driver
 *
--*/

#include <ndis.h>
#include <efilter.h>
#include "lancehrd.h"
#include "lancesft.h"

#if DBG
INT LanceDbg = 1;
INT LanceHLDbg = 1;
INT LancePMDbg = 0;
INT LanceSendDbg = 0;
INT LanceExtPhyDbg = 0;
INT LanceEventDbg = 0;
INT LanceRxDbg = 0;
INT LanceTxQueueDbg = 0;
INT LanceRegChkDbg = 0;
INT LanceREQDbg = 0;
INT LanceFilterDbg = 0;
INT LanceInitDbg = 1;
INT LanceTempDbg = 0;
INT LanceBreak = 0;
//jk #define STATIC

#if LANCELOG
NDIS_TIMER LogTimer;
BOOLEAN LogTimerRunning = FALSE;
UCHAR Log[LOG_SIZE];
UCHAR LogPlace = 0;
#endif

#endif

#if DBG	/* Used for Mutex macro in LANCESFT.H */
UCHAR Mutex;
#endif

/* Global data structures for the miniport.	*/
NDIS_HANDLE LanceWrapperHandle;

CHAR *Copyright = "Copyright(c) 1994-1999 AMD INC.";

/* Function prototypes	*/
STATIC
NDIS_STATUS
LanceInitialize(
	OUT PNDIS_STATUS OpenErrorStatus,
	OUT PUINT SelectedMediumIndex,
	IN PNDIS_MEDIUM MediumArray,
	IN UINT MediumArraySize,
	IN NDIS_HANDLE LanceMiniportHandle,
	IN NDIS_HANDLE ConfigurationHandle
	);

STATIC
NDIS_STATUS
LanceRegisterAdapter(
	IN NDIS_HANDLE LanceMiniportHandle,
	IN NDIS_HANDLE ConfigurationHandle,
	IN PUCHAR NetworkAddress,
	IN PLANCE_ADAPTER Adapter
	);

STATIC
ULONG
LanceHardwareDetails(
	IN PLANCE_ADAPTER Adapter,
	IN NDIS_HANDLE ConfigurationHandle
	);

STATIC
BOOLEAN
LanceCheckIoAddress(
	IN ULONG IoAddress
	);

STATIC
BOOLEAN
LanceCheckIrqDmaValid(
	IN PLANCE_ADAPTER Adapter
	);

STATIC
VOID
LanceCleanResources(
	IN PLANCE_ADAPTER Adapter
	);
/* JK 
STATIC
BOOLEAN
LanceROMCheck(
	IN ULONG PhysicalIoBaseAddress,
	IN NDIS_HANDLE ConfigurationHandle
	); */

STATIC
VOID
LanceScanPci(
	IN PLANCE_ADAPTER Adapter,
	IN NDIS_HANDLE ConfigurationHandle
	);

STATIC
VOID
LancePciEnableDma(
	IN PLANCE_ADAPTER Adapter
	);

STATIC
VOID
LancePciDisableDma(
	IN PLANCE_ADAPTER Adapter
	);


STATIC
VOID
LanceSetPciDma(
	IN PLANCE_ADAPTER Adapter,
	BOOLEAN EnablePciDma
	);

STATIC
NDIS_STATUS
LanceReset(
	OUT PBOOLEAN AddressingReset,
	IN NDIS_HANDLE MiniportAdapterContext
	);

STATIC
VOID
LanceShutdownHandler(
	IN PVOID Context
	);

STATIC
VOID
LanceHalt(
	IN NDIS_HANDLE Context
	);

STATIC
NDIS_STATUS
LanceTransferData(
	OUT PNDIS_PACKET Packet,
	OUT PUINT BytesTransferred,
	IN NDIS_HANDLE MiniportAdapterContext,
	IN NDIS_HANDLE MiniportReceiveContext,
	IN UINT ByteOffset,
	IN UINT BytesToTransfer
	);

STATIC
VOID
LanceInitAdapterType (
	IN PLANCE_ADAPTER Adapter
	);

STATIC
ULONG
LanceGetPciIoAddress (
	IN PLANCE_ADAPTER Adapter
	);

STATIC
UCHAR
LanceGetPciIntVector (
	IN PLANCE_ADAPTER Adapter
	);

STATIC
VOID
InitLEDs (
	IN PLANCE_ADAPTER Adapter
	);

STATIC
VOID
InitFullDuplexMode (
	IN PLANCE_ADAPTER Adapter
	);

STATIC
VOID
LanceSetExtPhyMedia(
	IN ULONG IoBaseAddress,
	IN ULONG ExtPhyMode
	);

VOID
LanceLinkMonitor(
	IN PVOID	SystemSpecific1,
	IN PLANCE_ADAPTER Adapter,
	IN PVOID	SystemSpecific2,
	IN PVOID	SystemSpecific3
	);

#ifdef NDIS40_PLUS

STATIC
VOID
LanceFreeNdisPkts (
	IN PLANCE_ADAPTER Adapter
	);

STATIC
BOOLEAN
LanceInitRxPacketPool(
	IN PLANCE_ADAPTER Adapter
	);

STATIC
VOID
LanceFreeRxPacketPool(
	IN PLANCE_ADAPTER Adapter
	);

#endif /* NDIS40_PLUS */


#ifdef NDIS50_PLUS

VOID
LanceInitPMR(
	PLANCE_ADAPTER Adapter
	);

STATIC
ULONG
LanceGetPCICapabilityID (
	IN PLANCE_ADAPTER Adapter
	);

#endif //NDIS50_PLUS

/****************************************************************************
			Home Lan Functions prototype
****************************************************************************/
BOOLEAN
IsExtPhyExist(
	IN ULONG IoBaseAddress,
	IN USHORT PhyAddress
	);

VOID
LanceFindPhyList(
	PLANCE_ADAPTER Adapter
	);

VOID
LanceDisableExtPhy(
	IN ULONG IoBaseAddress,
	IN USHORT PhyAddress
	);

VOID
LanceEnableExtPhy(
	IN ULONG IoBaseAddress,
	IN USHORT PhyAddress
	);

VOID
LanceIsolatePhys(
	PLANCE_ADAPTER Adapter
	);

VOID
LanceFindActivePhy(
	PLANCE_ADAPTER Adapter
	);

BOOLEAN
HomeLanPhyLinkStatus (
	IN ULONG IoBaseAddress,
	IN USHORT PhyAddress
	);

VOID
LanceHomeLanLinkMonitor(
	IN PVOID SystemSpecific1,
	IN PLANCE_ADAPTER Adapter,
	IN PVOID SystemSpecific2,
	IN PVOID SystemSpecific3
	);

VOID
CheckSfexPhySpeed(
	IN PLANCE_ADAPTER Adapter
	);

VOID
ProgramHomeRunState(
	IN PLANCE_ADAPTER Adapter,
	IN USHORT	HLState
	);

VOID
HomeLanMonitor(
	IN PVOID	SystemSpecific1,
	IN PLANCE_ADAPTER Adapter,
	IN PVOID	SystemSpecific2,
	IN PVOID	SystemSpecific3
	);

USHORT
HomeLanSyncRate(
	PLANCE_ADAPTER Adapter
	);

VOID
HomeRunPhySetting(
	PLANCE_ADAPTER Adapter
	);

VOID
LanceDeferredProcess(
	IN PVOID SystemSpecific1,
	IN PLANCE_ADAPTER Adapter,
	IN PVOID SystemSpecific2,
	IN PVOID SystemSpecific3
	);

VOID
LinkPurseControl(
	IN PLANCE_ADAPTER Adapter
	);

// jk Added this fucntion , a subset of LanceHardwareDetails 
STATIC
VOID
SetNetworkID(
   IN PLANCE_ADAPTER Adapter,
   IN NDIS_HANDLE ConfigurationHandle
   );

/*************************************************************************/


NTSTATUS
DriverEntry(
	IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING RegistryPath
	)

/*++

Routine Description:

	This is the primary initialization routine for the Lance driver.
	It is simply responsible for the intializing the wrapper and registering
	the MAC.	It then calls a system and architecture specific routine that
	will initialize and register each adapter.

Arguments:

	DriverObject - Pointer to driver object created by the system.

Return Value:

	The status of the operation.

--*/

{

	//
	// Receives the status of the NdisRegisterMac operation.
	//
	NDIS_STATUS Status;

	//
	// Allocate memory for data structure
	//
	NDIS_MINIPORT_CHARACTERISTICS LanceChar;

	#if DBG
		if (LanceDbg)
		{
			DbgPrint("==>DriverEntry\n");
			if (LanceBreak)
				//DbgBreakPoint();
				_asm int 3;
		}
	#endif

	//
	// Initialize the wrapper.
	//
	NdisMInitializeWrapper(
		&LanceWrapperHandle,
		DriverObject,
		RegistryPath,
		NULL
		);
	
	NdisZeroMemory(&LanceChar, sizeof(NDIS_MINIPORT_CHARACTERISTICS));

	//
	// Initialize the MAC characteristics for the call to
	// NdisRegisterMac.
	//
	LanceChar.MajorNdisVersion			= LANCE_NDIS_MAJOR_VERSION;
	LanceChar.MinorNdisVersion			= LANCE_NDIS_MINOR_VERSION;
	LanceChar.CheckForHangHandler		= NULL; //LanceCheckForHang;
	LanceChar.DisableInterruptHandler	= LanceDisableInterrupt;
	LanceChar.EnableInterruptHandler	= LanceEnableInterrupt;
	LanceChar.HaltHandler				= LanceHalt;
	LanceChar.HandleInterruptHandler	= LanceHandleInterrupt;
	LanceChar.InitializeHandler			= LanceInitialize;
	LanceChar.ISRHandler				= LanceISR;
	LanceChar.QueryInformationHandler	= LanceQueryInformation;
	LanceChar.ReconfigureHandler		= NULL;
	LanceChar.ResetHandler				= LanceReset;
	LanceChar.SetInformationHandler		= LanceSetInformation;
	LanceChar.TransferDataHandler		= LanceTransferData;

#ifdef NDIS40_PLUS

	// Extensions for NDIS 4.0
	// Keep the fields above consistent with NDIS 3.0 miniport characteristics
	//
	LanceChar.ReturnPacketHandler		= LanceReturnPacket;
	LanceChar.AllocateCompleteHandler	= NULL;
	LanceChar.SendHandler				= NULL;
	LanceChar.SendPacketsHandler		= LanceSendPackets;

#else

//	LanceChar.ReturnPacketHandler		= NULL;
//	LanceChar.AllocateCompleteHandler	= NULL;
	LanceChar.SendHandler				= LanceSend;
//	LanceChar.SendPacketsHandler		= NULL;

#endif

	//
	// Register the miniport driver
	//
	Status = NdisMRegisterMiniport(
		LanceWrapperHandle,
		&LanceChar,
		sizeof(LanceChar)
		);

	//
	// If ok, done
	//
	if (Status == NDIS_STATUS_SUCCESS)
	{
		#if DBG
		if (LanceDbg)
			DbgPrint("<==DriverEntry\n");
		#endif

		return Status;
	}

	//
	// We can only get here if something went wrong with registering
	// the miniport driver or *all* of the adapters.
	//
	NdisTerminateWrapper(LanceWrapperHandle, NULL);

	#if DBG
		if (LanceDbg)
		DbgPrint("DriverEntry Fail, NdisMRegisterMiniport\n");
	#endif

	return NDIS_STATUS_FAILURE;

}



STATIC
NDIS_STATUS
LanceInitialize(
	OUT PNDIS_STATUS OpenErrorStatus,
	OUT PUINT SelectedMediumIndex,
	IN PNDIS_MEDIUM MediumArray,
	IN UINT MediumArraySize,
	IN NDIS_HANDLE LanceMiniportHandle,
	IN NDIS_HANDLE ConfigurationHandle
	)

/*++

Routine Description:

	Initialize an adapter.

Arguments:

	OpenErrorStatus - Extra status bytes for opening adapters.

	SelectedMediumIndex - Index of the media type chosen by the driver.

	MediumArray - Array of media types for the driver to chose from.

	MediumArraySize - Number of entries in the array.

	LanceMiniportHandle - Handle for passing to the wrapper when
		referring to this adapter.

	ConfigurationHandle - A handle to pass to NdisOpenConfiguration.

Return Value:

	NDIS_STATUS_SUCCESS
	NDIS_STATUS_PENDING

--*/

{

	//
	// Text id strings
	//
	NDIS_STRING NetworkAddressString = NDIS_STRING_CONST("NETADDRESS");
	NDIS_STRING BaseAddressString = NDIS_STRING_CONST("IOAddress");
	NDIS_STRING InterruptVectorString = NDIS_STRING_CONST("Interrupt");
	NDIS_STRING DmaChannelString = NDIS_STRING_CONST("DmaChannel");
	NDIS_STRING BusTypeString = NDIS_STRING_CONST("BusType");
	NDIS_STRING ExternalPhyModeString = NDIS_STRING_CONST("EXTPHY");
	NDIS_STRING LED0String = NDIS_STRING_CONST("LED0");
	NDIS_STRING LED1String = NDIS_STRING_CONST("LED1");
	NDIS_STRING LED2String = NDIS_STRING_CONST("LED2");
	NDIS_STRING LED3String = NDIS_STRING_CONST("LED3");
	NDIS_STRING SlotNumberString = NDIS_STRING_CONST("SlotNumber");
	NDIS_STRING BusNumberString = NDIS_STRING_CONST("BusNumber");
// for HomeLan
	NDIS_STRING PhySelectionString = NDIS_STRING_CONST("PhySelected");
	NDIS_STRING HomeRunStateString = NDIS_STRING_CONST("HomeLan");
	NDIS_STRING HomeRunLinkOffString = NDIS_STRING_CONST("HomeLanLinkOff");
	NDIS_STRING HomeRunBackOffString = NDIS_STRING_CONST("HomeLanBackOff");
	NDIS_STRING HomeRunIPGString = NDIS_STRING_CONST("HomeLanIPG");
	NDIS_STRING HomeRunRateString = NDIS_STRING_CONST("SyncRate");
	NDIS_STRING MAX_CRC_per_SECString = NDIS_STRING_CONST("CRCperSec");
	NDIS_STRING HomeLanBugFixString = NDIS_STRING_CONST("HomeLanBugFix");
	NDIS_STRING LevelCtrlString = NDIS_STRING_CONST("LevelCtrl");
	NDIS_STRING AnalogTestString = NDIS_STRING_CONST("AnalogTest");
	NDIS_STRING HomeRun0String = NDIS_STRING_CONST("HomeRun0");
	NDIS_STRING HomeRun1String = NDIS_STRING_CONST("HomeRun1");
//	NDIS_STRING MAX_CONSECUTIVE_CRCString = NDIS_STRING_CONST("ConsecutiveCRC");

	//
	// Local variables
	//
	NDIS_STATUS Status;
	NDIS_HANDLE ConfigHandle;
	PNDIS_CONFIGURATION_PARAMETER ReturnedValue;
	UCHAR NetAddress[ETH_LENGTH_OF_ADDRESS];
	PLANCE_ADAPTER Adapter;
	UCHAR sum, count;
	PUCHAR NetworkAddress;
	UINT NetworkAddressLength;
	//jk USHORT	Data;

	//
	// Set break point here.	This routine may be called
	// several times depending on the number of device entries
	// inside registry
	//
	#if DBG
		if (LanceDbg)
			DbgPrint("==>LanceInitialize\n");
	#endif

	/* Search for correct medium.	*/
	for (; MediumArraySize > 0; MediumArraySize--)
	{
		if (MediumArray[MediumArraySize - 1] == NdisMedium802_3)
		{
			MediumArraySize--;
			break;
		}
	}
	#if DBG
		if (LanceBreak)
			_asm int 3;
	#endif

	if (MediumArray[MediumArraySize] != NdisMedium802_3)
	{
#if DBG
		if (LanceDbg)
			DbgPrint("<==LanceInitialize\nABORT: NDIS_STATUS_UNSUPPORTED_MEDIA\n");
#endif
		return NDIS_STATUS_UNSUPPORTED_MEDIA;
	}

	*SelectedMediumIndex = MediumArraySize;

	/* Allocate the adapter block	*/
	LANCE_ALLOC_MEMORY(&Status, &Adapter, sizeof(LANCE_ADAPTER));

	if (Status == NDIS_STATUS_SUCCESS)
	{
		NdisZeroMemory(Adapter, sizeof(LANCE_ADAPTER));
	
		/* Save driver handles	*/
		Adapter->LanceMiniportHandle = LanceMiniportHandle;
		Adapter->LanceWrapperHandle = LanceWrapperHandle;

		/* Set default values.	All the fields are initialized	*/
		/* to zero by NdisZeroMemory before	*/
		Adapter->led0 = LED_DEFAULT;
		Adapter->led1 = LED_DEFAULT;
		Adapter->led2 = LED_DEFAULT;
		Adapter->led3 = LED_DEFAULT;
		Adapter->LanceHardwareConfigStatus = LANCE_INIT_OK;
		Adapter->LineSpeed = LINESPEED_DEFAULT;			

//for HomeLan
		Adapter->ActivePhyAddress = SFEX_PHY_ADDRESS;
		Adapter->PhySelected = (ULONG)FIND_ACTIVE_PHY; // Auto link find

//for DMI
#ifdef DMI
		Adapter->DmiIdRev[0] = 'D';
		Adapter->DmiIdRev[1] = 'M';
		Adapter->DmiIdRev[2] = 'I';
		Adapter->DmiIdRev[3] = ADAPTER_STRUCTURE_DMI_VERSION;	
#endif //DMI
	}
	else
	{
		#if DBG
		if (LanceDbg)
			DbgPrint("<==LanceInitialize\nABORT: Failed allocating adapter memory!\n");
		#endif
		return NDIS_STATUS_RESOURCES;
	}

	/* Open the configuration info	*/
	NdisOpenConfiguration(
		&Status,
		&ConfigHandle,
		ConfigurationHandle
		);

	if (Status != NDIS_STATUS_SUCCESS)
	{
		#if DBG
		if (LanceDbg)
			DbgPrint("<==LanceInitialize\nABORT: Failed opening configuration registry!\n");
		#endif
		return Status;
	}

	/* Get the Slot Number Keyword	*/
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&SlotNumberString,
		NdisParameterHexInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->LanceSlotNumber = ReturnedValue->ParameterData.IntegerData;
	}
	else
	{
		#if DBG
		if (LanceDbg)
			DbgPrint("Slot number not read. NdisReadConfiguration returned %8.8X\n", Status);
		#endif
	}

	/* Get the Bus Number Keyword	*/
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&BusNumberString,
		NdisParameterHexInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->LanceBusNumber = ReturnedValue->ParameterData.IntegerData;
	}
	else
	{
		#if DBG
		if (LanceDbg)
			DbgPrint("Bus number not read. NdisReadConfiguration returned %8.8X\n", Status);
		#endif
	}

	#if DBG
		if (LanceDbg)
		DbgPrint("SLOT NUMBER = %x\n",Adapter->LanceSlotNumber);
	#endif

	//
	// Get the interrupt vector number
	//
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&InterruptVectorString,
		NdisParameterInteger
		);
	
	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->LanceInterruptVector = 
		(CCHAR)ReturnedValue->ParameterData.IntegerData;
	}

	/* Get the DMA channel number	*/
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&DmaChannelString,
		NdisParameterInteger
		);
	
	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->LanceDmaChannel = 
			(CCHAR)ReturnedValue->ParameterData.IntegerData;
	}
	
	/* Get the IO base address	*/
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&BaseAddressString,
		NdisParameterHexInteger
		);
	
	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->PhysicalIoBaseAddress = ReturnedValue->ParameterData.IntegerData;
	}

	//
	// Get the bus type
	//
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&BusTypeString,
		NdisParameterInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->BusType = (NDIS_INTERFACE_TYPE)ReturnedValue->ParameterData.IntegerData;
	}
	//jk hard coded the BusType = PCIBus. To be removed once the driver is expanded 
	// to add more bus support e.g ISA, ISAII ext 
	Adapter->BusType = PCIBus;


	#if DBG
	if (LanceDbg)
	{
		DbgPrint("Registry information:\n");
		DbgPrint("IO %x\n", Adapter->PhysicalIoBaseAddress);
		DbgPrint("INT %d\n", Adapter->LanceInterruptVector);
		DbgPrint("DMA %d\n", Adapter->LanceDmaChannel);
	}
	#endif

// NDIS3 also need to use this keyword!!!
//#ifdef NDIS40_PLUS

/***********************************
 *	Get the External Phy Mode keyword *
 ***********************************/

	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&ExternalPhyModeString,
		NdisParameterInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->ExtPhyMode = ReturnedValue->ParameterData.IntegerData;
#if DBG
			if (LanceDbg)
				DbgPrint("EXTPHY = %x\n",Adapter->ExtPhyMode);
#endif

	}
//#endif /* NDIS40_PLUS */

	//
	// Get the LED0 keyword
	//
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&LED0String,
		NdisParameterHexInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->led0 = ReturnedValue->ParameterData.IntegerData;
		if (Adapter->led0 == 0x10000)
		{
			Adapter->led0 = LED_DEFAULT;
		}
	}

	#if DBG
		if (LanceDbg)
		DbgPrint("LED0	%x\n", Adapter->led0);
	#endif

	//
	// Get the LED1 keyword
	//
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&LED1String,
		NdisParameterHexInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->led1 = ReturnedValue->ParameterData.IntegerData;
		if (Adapter->led1 == 0x10000)
		{
			Adapter->led1 = LED_DEFAULT;
		}
	}

	#if DBG
		if (LanceDbg)
		DbgPrint("LED1	%x\n", Adapter->led1);
	#endif

	//
	// Get the LED2 keyword
	//
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&LED2String,
		NdisParameterHexInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->led2 = ReturnedValue->ParameterData.IntegerData;
		if (Adapter->led2 == 0x10000)
		{
			Adapter->led2 = LED_DEFAULT;
		}
	}

	#if DBG
		if (LanceDbg)
		DbgPrint("LED2	%x\n", Adapter->led2);
	#endif

	//
	// Get the LED3 keyword
	//
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&LED3String,
		NdisParameterHexInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->led3 = ReturnedValue->ParameterData.IntegerData;
		if (Adapter->led3 == 0x10000)
		{
			Adapter->led3 = LED_DEFAULT;
		}
	}

	#if DBG
		if (LanceDbg)
		DbgPrint("LED3	%x\n", Adapter->led3);
	#endif

// for HomeLan
/*************************************************************
			 Get the HomeRun State Parameter
*************************************************************/

	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&HomeRunStateString,
		NdisParameterInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		switch (ReturnedValue->ParameterData.IntegerData) {
			case 0 :
				Adapter->isAutoConfigOn = TRUE;
			case 2 :
				Adapter->HomeRunState = STATE_LPHS;
				break;
			case 1 :
				Adapter->isAutoConfigOn = TRUE;
			case 3 :
				Adapter->HomeRunState = STATE_HPHS;
				break;
			case 4 :
				Adapter->HomeRunState = STATE_HPLS;
				break;
				
			default :
				Adapter->HomeRunState = STATE_LPHS;
				break;
		}
	}
	else
		Adapter->HomeRunState = STATE_LPHS;

	#if DBG
		if (LanceHLDbg)
		DbgPrint("HomeRun State	%d\n", Adapter->HomeRunState);
	#endif

	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&HomeRunLinkOffString,
		NdisParameterInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->HLLinkOff = ReturnedValue->ParameterData.IntegerData;
	}
	#if DBG
		if (LanceHLDbg)
		DbgPrint("LinkOff	%d\n", Adapter->HLLinkOff);
	#endif

	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&HomeRunBackOffString,
		NdisParameterInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->HLBackOff = ReturnedValue->ParameterData.IntegerData;
	}
	#if DBG
		if (LanceHLDbg)
		DbgPrint("BackOff	%d\n", Adapter->HLBackOff);
	#endif

	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&HomeRunIPGString,
		NdisParameterInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->IPG = ReturnedValue->ParameterData.IntegerData;
	}
	else
		Adapter->IPG = 0x0064;

	#if DBG
		if (LanceHLDbg)
		DbgPrint("IPG	%d\n", Adapter->IPG);
	#endif

	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&HomeRunRateString,
		NdisParameterInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->SyncRate = ReturnedValue->ParameterData.IntegerData;
	}
	else
		Adapter->SyncRate = TRUE;

	#if DBG
		if (LanceHLDbg)
		DbgPrint("SyncRate %d\n", Adapter->SyncRate);
	#endif

	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&HomeLanBugFixString,
		NdisParameterInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->RunBugFix = (BOOLEAN)ReturnedValue->ParameterData.IntegerData;
	}
	#if DBG
		if (LanceHLDbg)
		DbgPrint("BugFix	%d\n", Adapter->RunBugFix);
	#endif

/*****************************************************************************
			 Get the HomeRun Phy Register programming parameters
*****************************************************************************/
//for HomeRun Reg30 Modification
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&LevelCtrlString,
		NdisParameterInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->LevelCtrl = (USHORT)ReturnedValue->ParameterData.IntegerData;
	}

//for HomeRun Reg31 Modification
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&AnalogTestString,
		NdisParameterInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->AnalogTest = (USHORT)ReturnedValue->ParameterData.IntegerData;
	}

//for HomeRun Phy Register Modification
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&HomeRun0String,
		NdisParameterInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->HomeRun0 = (ULONG)ReturnedValue->ParameterData.IntegerData;
	}

//for HomeRun Phy Register Modification
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&HomeRun1String,
		NdisParameterInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->HomeRun1 = (ULONG)ReturnedValue->ParameterData.IntegerData;
	}

/*************************************************************
			 Get the HomeRun CRC per Sec parameter
*************************************************************/

	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&MAX_CRC_per_SECString,
		NdisParameterInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->HLAC.MaxAllowedCRCs = ReturnedValue->ParameterData.IntegerData;
	}
	else
	{
		Adapter->HLAC.MaxAllowedCRCs = DEFAULT_CRC_PER_SEC;
	}

	#if DBG
		if (LanceHLDbg)
		DbgPrint("Max CRC/sec %d\n", Adapter->HLAC.MaxAllowedCRCs);
	#endif


	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&PhySelectionString,
		NdisParameterInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->PhySelected = ReturnedValue->ParameterData.IntegerData;
	}

	#if DBG
		if (LanceHLDbg)
		DbgPrint("PhySelected %x\n", Adapter->PhySelected);
	#endif

/*************************************************************
			 Get Max Consecutive CRC Parameter
*************************************************************/
/*
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&MAX_CONSECUTIVE_CRCString,
		NdisParameterInteger
		);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Adapter->MaxConsecutiveCRCs = ReturnedValue->ParameterData.IntegerData;
	}
	else
	{
		Adapter->MaxConsecutiveCRCs = DEFAULT_CONSECUTIVE_CRC;
	}

	#if DBG
		if (LanceHLDbg)
		DbgPrint("Max Consecutive CRCs %d\n", Adapter->MaxConsecutiveCRCs);
	#endif

*/
	//
	// Read network address
	//
	NdisReadNetworkAddress(
		&Status,
		(PVOID *)&NetworkAddress,
		&NetworkAddressLength,
		ConfigHandle
		);

	//
	// Make sure that the address is the right length asnd
	// at least one of the bytes is non-zero.
	//
	if (Status == NDIS_STATUS_SUCCESS &&
		NetworkAddressLength == ETH_LENGTH_OF_ADDRESS)
	{
		sum = 0;
		for (count = 0; count < ETH_LENGTH_OF_ADDRESS; count++)
		{
			NetAddress[count] = *(NetworkAddress + count);
			sum |= NetAddress[count];
		}

		//
		// If network address is zero, set the address to NULL
		// so we are going to use the burned in address
		//
		NetworkAddress = sum ? NetAddress : NULL;

		#if DBG
		if(LanceDbg)
		{
			DbgPrint("Network Address %.x-%.x-%.x-%.x-%.x-%.x\n",
				(UCHAR)NetAddress[0],
				(UCHAR)NetAddress[1],
				(UCHAR)NetAddress[2],
				(UCHAR)NetAddress[3],
				(UCHAR)NetAddress[4],
				(UCHAR)NetAddress[5]);
		}
		#endif

	} else {

		//
		// Tells LanceRegisterAdapter to use the burned-in address.
		//
		NetworkAddress = NULL;

	}

	//
	// Used passed-in adapter name to register.
	//
	NdisCloseConfiguration(ConfigHandle);

	Status = LanceRegisterAdapter(
				LanceMiniportHandle,
				ConfigurationHandle,
				NetworkAddress,
				Adapter
				);

	//
	// Check status.	If not ok, release allocated memory and 
	// return error status
	//
	if (Status != NDIS_STATUS_SUCCESS)
	{
		LANCE_FREE_MEMORY(Adapter, sizeof(LANCE_ADAPTER));
		#if DBG
		if (LanceDbg)
			DbgPrint("<==LanceInitialize\nABORT: LanceRegisterAdapter returned %8.8X\n", Status);
		#endif

		return Status;
	}

	NdisMRegisterAdapterShutdownHandler(
				LanceMiniportHandle,
				(PVOID)Adapter,
				LanceShutdownHandler
				);

	NdisMInitializeTimer (
		&(Adapter->CableTimer),
		LanceMiniportHandle,
		(PVOID)LanceHomeLanLinkMonitor,
		(PVOID)Adapter
		);

	Adapter->CableDisconnected = FALSE;
	Adapter->LinkActive = FALSE;

#ifdef NDIS40_PLUS
	NdisMSetPeriodicTimer (&(Adapter->CableTimer),HOMELAN_TIMEOUT);
#else
	NdisMSetTimer(&(Adapter->CableTimer),HOMELAN_TIMEOUT);
#endif
	
	NdisMInitializeTimer (
			&(Adapter->DeferredProcessTimer),
			LanceMiniportHandle,
			(PVOID)LanceDeferredProcess,
			(PVOID)Adapter
			);

/* TxQueue */
	NdisAllocateSpinLock(&Adapter->TxQueueLock);
			
#if DBG
	if (LanceDbg)
		DbgPrint("<==LanceInitialize\n");
#endif

	return Status;

}


STATIC
NDIS_STATUS
LanceRegisterAdapter(
	IN NDIS_HANDLE LanceMiniportHandle,
	IN NDIS_HANDLE ConfigurationHandle,
	IN PUCHAR NetworkAddress,
	IN PLANCE_ADAPTER Adapter
	)

/*++

Routine Description:

	This routine is responsible for the allocation of the data structures
	for the driver as well as any hardware specific details necessary
	to talk with the device.

Arguments:

	LanceMiniportHandle - The driver handle.

	ConfigurationHandle - Config handle passed to MacAddAdapter.

	NetworkAddress - The network address, or NULL if the default
					should be used.

	Adapter - The pointer for the received information from registry

Return Value:

	Returns a failure status if anything occurred that prevents the
	initialization of the adapter.

--*/

{
	UCHAR i;
	NDIS_ERROR_CODE ErrorCode;
	NDIS_STATUS Status;
	ULONG HardwareDetailsStatus;
	//
	// Local Pointer to the Initialization Block.
	//
	PLANCE_INIT_BLOCK_HI InitializationBlockHi;
	//jk PNDIS_RESOURCE_LIST AssignedResources;
	USHORT	Data;
#if DBG
	USHORT TempData;
#endif

	#if DBG
		if (LanceDbg)
		DbgPrint("==>LanceRegisterAdapter\n");
		if (LanceBreak)
			_asm int 3;
	#endif

	//
	// We put in this assertion to make sure that ushort are 2 bytes.
	// if they aren't then the initialization block definition needs
	// to be changed.
	//
	// Also all of the logic that deals with status registers assumes
	// that control registers are only 2 bytes.
	//
	ASSERT(sizeof(USHORT) == 2);

	//
	// Get hardware resource information
	//
	HardwareDetailsStatus = LanceHardwareDetails(
												Adapter,
												ConfigurationHandle
												);

	//
	// Check status returned from routine
	//
	if (HardwareDetailsStatus != LANCE_INIT_OK)
	{
		switch (HardwareDetailsStatus)
		{
			case LANCE_INIT_ERROR_9 :
				// IO base address already in use by another device
			case LANCE_INIT_ERROR_13:
				// IRQ and/or DMA already in use by another device
				ErrorCode = NDIS_ERROR_CODE_RESOURCE_CONFLICT;
				break;

			case LANCE_INIT_ERROR_18:
				// PCI scan specified, device not found
			case LANCE_INIT_ERROR_21:
				// Device not found
			case LANCE_INIT_ERROR_22:
				// PnP scan specified, device not found
			case LANCE_INIT_ERROR_23:
				// VESA scan specified, device not found
			case LANCE_INIT_ERROR_24:
				// ISA scan specified, device not found
				ErrorCode = NDIS_ERROR_CODE_ADAPTER_NOT_FOUND;
				break;

			case LANCE_INIT_ERROR_20:
				// Device at specified IO base address not found
				ErrorCode = NDIS_ERROR_CODE_BAD_IO_BASE_ADDRESS;
				break;

			default:
				ErrorCode = NDIS_ERROR_CODE_DRIVER_FAILURE;
		}

		//
		// Log error and exit
		//
		NdisWriteErrorLogEntry(Adapter->LanceMiniportHandle,
							ErrorCode,
							0
							);
		#if DBG
		if (LanceDbg)
		{
			DbgPrint("LanceHarewareDetails return error status: ");
			DbgPrint("%x\n", HardwareDetailsStatus);
		}
		#endif

		//
		// Return with fail status
		//
		return NDIS_STATUS_FAILURE;

	}

	//
	// Register the adapter with NDIS.
	//
	NdisMSetAttributes(
					Adapter->LanceMiniportHandle,
					(NDIS_HANDLE)Adapter,
					TRUE,
					Adapter->BusType
					);

	//
	// Register the adapter io range
	//
	Status = NdisMRegisterIoPortRange(
						(PVOID *)(&(Adapter->MappedIoBaseAddress)),
						Adapter->LanceMiniportHandle,
						Adapter->PhysicalIoBaseAddress,
						0x20
						);

	//
	// Return error status if failed
	//
	if (Status != NDIS_STATUS_SUCCESS)
	{
		#if DBG
		if (LanceDbg)
			DbgPrint("Failed registering I/O address!\n");
		#endif

		//
		// Log error if io registation failed
		//
		NdisWriteErrorLogEntry(
			Adapter->LanceMiniportHandle,
			NDIS_ERROR_CODE_OUT_OF_RESOURCES,
			2,
			registerAdapter,
			LANCE_ERRMSG_HARDWARE_ADDRESS
			);

		goto InitErr0;
	}
	//JK This function sets the Network Address and Device Id 
	SetNetworkID(Adapter,ConfigurationHandle);
	/*jk end */

	//
	// Allocate memory for all of the adapter structures.
	//
	if (!LanceAllocateAdapterMemory(Adapter))
	{
		#if DBG
		if (LanceDbg)
			DbgPrint("Failed allocating memory for descriptor rings!\n");
		#endif

		//
		// Log error if memory allocation failed
		//
		NdisWriteErrorLogEntry(
		Adapter->LanceMiniportHandle,
		NDIS_ERROR_CODE_OUT_OF_RESOURCES,
		2,
		registerAdapter,
		LANCE_ERRMSG_ALLOC_MEMORY
		);

		Status = NDIS_STATUS_RESOURCES;
		goto InitErr1;

	}

	//
	// Initialize the current hardware address.
	//

	NdisMoveMemory(
		Adapter->CurrentNetworkAddress,
		(NetworkAddress != NULL) ? NetworkAddress : Adapter->PermanentNetworkAddress,
		ETH_LENGTH_OF_ADDRESS);

	//
	// Initialize the init-block structure for the adapter
	//
	InitializationBlockHi = (PLANCE_INIT_BLOCK_HI)Adapter->InitializationBlock;
	InitializationBlockHi->Mode = LANCE_NORMAL_MODE;

	//
	// Initialize the init-block network addresses
	//
	for (i = 0; i < ETH_LENGTH_OF_ADDRESS; i++)
	InitializationBlockHi->PhysicalNetworkAddress[i]
		= Adapter->CurrentNetworkAddress[i];
	for (i = 0; i < 8; i++)
	InitializationBlockHi->LogicalAddressFilter[i] = 0;

	//
	// Set the init-block receiving descriptor ring pointer low address
	//
	InitializationBlockHi->ReceiveDescriptorRingPhysicalLow
		= LANCE_GET_LOW_PART_ADDRESS(
			NdisGetPhysicalAddressLow(
				Adapter->ReceiveDescriptorRingPhysical));

	//
	// Set number of receiving descriptors in RLEN field
	//
	i = RECEIVE_BUFFERS;
	while (i >>= 1)
	InitializationBlockHi->RLen
		+= BUFFER_LENGTH_EXPONENT_H;

	//
	// Set the init-block receiving descriptor ring pointer high address
	//
	InitializationBlockHi->ReceiveDescriptorRingPhysicalHighL
		|= LANCE_GET_HIGH_PART_ADDRESS(
				NdisGetPhysicalAddressLow(
				Adapter->ReceiveDescriptorRingPhysical));

	InitializationBlockHi->ReceiveDescriptorRingPhysicalHighH
		|= LANCE_GET_HIGH_PART_ADDRESS_H(
				NdisGetPhysicalAddressLow(
				Adapter->ReceiveDescriptorRingPhysical));


	InitializationBlockHi->TransmitDescriptorRingPhysicalLow
		= LANCE_GET_LOW_PART_ADDRESS(
			NdisGetPhysicalAddressLow(Adapter->TransmitDescriptorRingPhysical));

	//
	// Set number of transmit descriptors in TLEN field
	//
	i = TRANSMIT_BUFFERS;
	while (i >>= 1)
	InitializationBlockHi->TLen
		+= BUFFER_LENGTH_EXPONENT_H;

	//
	// Set the init-block transmit descriptor ring pointer high address
	//
	InitializationBlockHi->TransmitDescriptorRingPhysicalHighL
		|= LANCE_GET_HIGH_PART_ADDRESS(
			NdisGetPhysicalAddressLow(
				Adapter->TransmitDescriptorRingPhysical));

	InitializationBlockHi->TransmitDescriptorRingPhysicalHighH
		|= LANCE_GET_HIGH_PART_ADDRESS_H(
			NdisGetPhysicalAddressLow(
				Adapter->TransmitDescriptorRingPhysical));

#ifdef NDIS40_PLUS
	if (!(LanceInitRxPacketPool (Adapter)))
	{
		#if DBG
		if (LanceDbg)
			DbgPrint("Failed allocating packets &/or buffers for receive!\n");
		#endif

		//
		// Log error if buffer/packet allocation failed
		//
		NdisWriteErrorLogEntry(
		Adapter->LanceMiniportHandle,
		NDIS_ERROR_CODE_OUT_OF_RESOURCES,
		2,
		registerAdapter,
		LANCE_ERRMSG_HARDWARE_ALLOC_BUFFER
		);

		Status = NDIS_STATUS_RESOURCES;
		goto InitErr1;
	}
#endif

	NdisZeroMemory (Adapter->GeneralMandatory, GM_ARRAY_SIZE * sizeof(ULONG));
	NdisZeroMemory (Adapter->GeneralOptionalByteCount, GO_COUNT_ARRAY_SIZE * sizeof(LANCE_LARGE_INTEGER));
	NdisZeroMemory (Adapter->GeneralOptionalFrameCount, GO_COUNT_ARRAY_SIZE * sizeof(ULONG));
	NdisZeroMemory (Adapter->GeneralOptional, (GO_ARRAY_SIZE - GO_ARRAY_START) * sizeof(ULONG));
	NdisZeroMemory (Adapter->MediaMandatory, MM_ARRAY_SIZE * sizeof(ULONG));
	NdisZeroMemory (Adapter->MediaOptional, MO_ARRAY_SIZE * sizeof(ULONG));

#ifdef NDIS50_PLUS
	if (Adapter->FuncFlags & CAP_POWER_MANAGEMENT)
	{
	 	LanceInitPMR(Adapter);
	}
#endif //NDIS50_PLUS

	/* TxQueue */
	// Initialize the Transmit queueing pointers to NULL
	Adapter->FirstTxQueue = (PNDIS_PACKET) NULL;
	Adapter->LastTxQueue = (PNDIS_PACKET) NULL;
	Adapter->NumPacketsQueued = 0;

	/* Initialize the interrupt.	*/
	NdisZeroMemory (&Adapter->LanceInterruptObject, sizeof(NDIS_MINIPORT_INTERRUPT));

	/* Save interrupt mode	*/
	Adapter->InterruptMode = NdisInterruptLevelSensitive;

	Status = NdisMRegisterInterrupt(
				(PNDIS_MINIPORT_INTERRUPT)&(Adapter->LanceInterruptObject),
					Adapter->LanceMiniportHandle,
					(UINT)Adapter->LanceInterruptVector,
					(UINT)Adapter->LanceInterruptVector,
					FALSE,
					TRUE,
					Adapter->InterruptMode
					);

	if (Status != NDIS_STATUS_SUCCESS)
	{
		#if DBG
		if (LanceDbg)
			DbgPrint("Failed registering interrupt!\n");
		#endif

		/* Log error information	*/
		NdisWriteErrorLogEntry(
			Adapter->LanceMiniportHandle,
			NDIS_ERROR_CODE_INTERRUPT_CONNECT,
			2,
			registerAdapter,
			LANCE_ERRMSG_INIT_INTERRUPT
			);

		goto InitErr2;
	}

	Adapter->FuncFlags |= CAP_HOME_LAN;

	#if DBG
	if (LanceHLDbg)
		DbgPrint("Adapter FuncFlags : %x\n", Adapter->FuncFlags);
	#endif

	CheckSfexPhySpeed(Adapter);
	
	LanceHomeLanPortScan(Adapter);

	if (Adapter->ActivePhyAddress == HOMERUN_PHY_ADDRESS)
	{
		InitHomeRunPort(Adapter);

		if(Adapter->DeviceType == PCNET_HOME_A1)
		{
			Adapter->RunBugFix = 1;

			if (!Adapter->LevelCtrl)
			{
				Adapter->LevelCtrl = DEFAULT_LEVEL_CTRL;
			}
		}

		// Program IPG to 0x64xx (Errata #1 for PCnet_HL) 
		LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR125, &Data);
		Data = (Data & IPGMASK)|((USHORT)Adapter->IPG<<8);
		LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR125, Data);
	}
	else
	{
		//Initialize SFEX IADJ register to 1010 (Reg 28 : 0x0500)
		if (Adapter->ActivePhyAddress == SFEX_PHY_ADDRESS)
		{
			LANCE_WRITE_MII(Adapter->MappedIoBaseAddress,
						(Adapter->ActivePhyAddress | 28), 0x0500);
		}
		Adapter->isAutoConfigOn = FALSE;
		HomeLanExtPhyConfig(Adapter);
	}

		#if DBG
			if (LanceHLDbg)
			{
				DbgPrint("Active Phy : %x, DefaultPhy : %x\n", 
							Adapter->ActivePhyAddress,Adapter->DefaultPhyAddress);
				LANCE_READ_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR32, &Data);
				DbgPrint("BCR32 : %x\n",Data);
				LANCE_READ_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR33, &Data);
				DbgPrint("BCR33 : %x\n",Data);
				LANCE_READ_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR34, &Data);
				DbgPrint("BCR34 : %x\n",Data);

			}
		#endif


	#ifdef DBG
		if(LanceDbg)
		{	for (i=0; i<(UCHAR)Adapter->NumberOfPhys; i++)
				for (Data=0; Data<32; Data++)
				{
					LANCE_WRITE_BCR (Adapter->MappedIoBaseAddress, MII_ADDR, 
									(Adapter->PhyAddress[i] | Data));
					LANCE_READ_BCR (Adapter->MappedIoBaseAddress, MII_MDR, &TempData);
					DbgPrint("PHY %x, REG %x : %x\n",Adapter->PhyAddress[i],Data,TempData);
				}

			LANCE_READ_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR49, &Data);
			DbgPrint("BCR49 %x\n",Data);
			
		}
	#endif

	if ((Adapter->ActivePhyAddress == HOMERUN_PHY_ADDRESS)&&(Adapter->RunBugFix))
		EverestBugFix(Adapter);

	/* Initialize chip	*/
	#if DBG
	if (LanceInitDbg)
		DbgPrint("LanceRisterAdapter calls LanceInit.\n");
	#endif
	LanceInit(Adapter, TRUE);

	//
	// Set flag for initialization completion
	//
	Adapter->OpFlags |= INIT_COMPLETED;

	#if DBG
		if (LanceDbg)
		DbgPrint("<==LanceRegisterAdapter\n");
	#endif

	return NDIS_STATUS_SUCCESS;

	/* Error handler code	*/
InitErr2:

	LanceDeleteAdapterMemory(Adapter);

InitErr1:

	//
	// Deregister IO addresses
	//
	NdisMDeregisterIoPortRange(
		Adapter->LanceMiniportHandle,
		Adapter->PhysicalIoBaseAddress,
		0x20,
		(PVOID)(Adapter->MappedIoBaseAddress)
		);

InitErr0:

	return Status;

}

// JK This function has been added 
STATIC
VOID
SetNetworkID(
   IN PLANCE_ADAPTER Adapter,
   IN NDIS_HANDLE ConfigurationHandle
   )
{
	USHORT  Data;
	UCHAR dataByte;
	int i; 

   // Save network adapter address
   //
    
   for (i = 0; i < 6; i++) {
     
      NdisRawReadPortUchar(Adapter->MappedIoBaseAddress + i, &dataByte);                                                                 

      Adapter->PermanentNetworkAddress[i] = dataByte;

   }

   #if DBG
       if (LanceDbg) {
	 DbgPrint("[ %.x-%.x-%.x-%.x-%.x-%.x ] ",
		 (UCHAR)Adapter->PermanentNetworkAddress[0],
		 (UCHAR)Adapter->PermanentNetworkAddress[1],
		 (UCHAR)Adapter->PermanentNetworkAddress[2],
		 (UCHAR)Adapter->PermanentNetworkAddress[3],
		 (UCHAR)Adapter->PermanentNetworkAddress[4],
		 (UCHAR)Adapter->PermanentNetworkAddress[5]);
	 DbgPrint("\n");
       }
   #endif
    
   //
   // Reset the controller and stop the chip
   //
    
   NdisRawReadPortUshort(Adapter->MappedIoBaseAddress + LANCE_RESET_PORT, 
			&Data);
			
   //
   // Delay after reset
   //
   NdisStallExecution(500);

   return;
}


STATIC
ULONG
LanceHardwareDetails(
	IN PLANCE_ADAPTER Adapter,
	IN NDIS_HANDLE ConfigurationHandle
	)

/*++

Routine Description:

	This routine scans for PCNET family devices and gets the resources
	for the AMD hardware.

	ZZZ This routine is *not* portable.	It is specific to NT and
	to the Lance implementation.

Arguments:

	Adapter - Where to store the network address, base address, irq and dma.

Return Value:

	LANCE_INIT_OK if successful, LANCE_INIT_ERROR_<NUMBER> if failed,
	LANCE_INIT_WARNING_<NUMBER> if we continue but need to warn user.


--*/

{

	//jk ULONG i;
	//jk ULONG status;
	//jk UCHAR dataByte;
	//jk USHORT Data; //jk Id
	//jk ULONG DeviceId;

	#if DBG
		if (LanceDbg)
		DbgPrint("==>LanceHardwareDetails\n");
	#endif

	//
	// Search device based on user's choice and bus type
	// information detected and saved inside the registry
	//
	if (Adapter->BusType == NdisInterfacePci)
	{
		//
		// Scan PCI bus to find controller
		//
		LanceScanPci (Adapter, ConfigurationHandle);
	}

	//
	// If no adapter found, return error status
	//
	if (!Adapter->DeviceType)
	{
		#if DBG
		if(LanceDbg)
			DbgPrint("HomeLan Board Not Found.\n");
		#endif
		return LANCE_INIT_ERROR_21;
	}

	#if DBG
	if (LanceDbg)
	{
		DbgPrint("IO Port %x\n",Adapter->PhysicalIoBaseAddress);
		DbgPrint("INT %d\n",Adapter->LanceInterruptVector);
		DbgPrint("DMA %d\n",Adapter->LanceDmaChannel);
	}
	#endif

	//
	// Save network adapter address
	//
/* JK Start 
	for (i = 0; i < 6; i++)
	{
		NdisImmediateReadPortUchar(ConfigurationHandle,
								   Adapter->PhysicalIoBaseAddress + i,
								   &dataByte);
		Adapter->PermanentNetworkAddress[i] = dataByte;
	}

	#if DBG
	if (LanceDbg)
	{
		DbgPrint("[ %.x-%.x-%.x-%.x-%.x-%.x ] ",
			(UCHAR)Adapter->PermanentNetworkAddress[0],
			(UCHAR)Adapter->PermanentNetworkAddress[1],
			(UCHAR)Adapter->PermanentNetworkAddress[2],
			(UCHAR)Adapter->PermanentNetworkAddress[3],
			(UCHAR)Adapter->PermanentNetworkAddress[4],
			(UCHAR)Adapter->PermanentNetworkAddress[5]);
		DbgPrint("\n");
	}
	#endif

	//
	// Reset the controller and stop the chip
	//
	NdisImmediateReadPortUshort(ConfigurationHandle,
								Adapter->PhysicalIoBaseAddress + LANCE_RESET_PORT, 
								&Data);

	//
	// Delay after reset
	//
	NdisStallExecution(500);
 JK END */
	#if DBG
		if (LanceDbg)
		DbgPrint("<==LanceHardwareDetails\n");
	#endif

	return Adapter->LanceHardwareConfigStatus;

}

/* Jk  - Commented out this function as it is not used for HomeLan PCI device /*

STATIC
BOOLEAN
LanceROMCheck(
	IN ULONG PhysicalIoBaseAddress,
	IN NDIS_HANDLE ConfigurationHandle
	)

/*++

Routine Description:

	This routine checks the 'WW' signature and checksum from the hardware.

	ZZZ This routine is *not* portable.	It is specific to NT and
	to the Lance implementation.

Arguments:

	PhysicalIoBaseAddress - io address to check.

Return Value:

	TRUE if successful.

--*/
/* JK START
{

	UINT i;
	UCHAR Data;
	USHORT CheckSum = 0;
	USHORT StoredCheckSum;

	#if DBG
		if (LanceDbg)
		DbgPrint("==>LanceRomCheck\n");
	#endif

	//
	// Calculate the checksum of bytes 00h - 0Bh from the PROM.
	//
	for (i = 0; i < 12; i++)
	{
		NdisImmediateReadPortUchar(ConfigurationHandle,
								PhysicalIoBaseAddress + i,
								&Data);
		CheckSum += Data;
	}

	//
	// Specific to certain HW.
	//
	if (PhysicalIoBaseAddress == 0x8800)
	{
		NdisImmediateReadPortUchar(ConfigurationHandle,
								PhysicalIoBaseAddress + 8,
								&Data);
		CheckSum -= Data;
	}

	//
	// Calculate the checksum of bytes 0Eh - 0Fh from the PROM.
	//
	for (i = 14; i < 16; i++)
	{
		NdisImmediateReadPortUchar(ConfigurationHandle,
								PhysicalIoBaseAddress + i,
								&Data);

		CheckSum += Data;

		//
		// Check for the ASCI character "W".
		//
		if (Data != 'W')
		{
			return FALSE;
		}

	}

	//
	// Read saved checksum vale from PROM
	//
	NdisImmediateReadPortUshort(ConfigurationHandle,
								PhysicalIoBaseAddress + 12,
								&StoredCheckSum);

	#if DBG
		if (LanceDbg)
		DbgPrint("<==LanceRomCheck\n");
	#endif

	//
	// Return status
	//
	return (CheckSum == StoredCheckSum ? TRUE : FALSE);

}
JK END */

STATIC
VOID
LanceScanPci (
	IN PLANCE_ADAPTER Adapter,
	IN NDIS_HANDLE ConfigurationHandle
	)

/*++

Routine Description:

	This routine detects PCI device.	If device is found, the device
	parameters such as io address and interrupt number are saved.

Arguments:

	Adapter - Driver data storage pointer

	ConfigurationHandle - Configuration handle

Return Value:

	None.

--*/

{

	ULONG Buffer;
	int i = 0; //jk added
	#if DBG
		if (LanceDbg)
		DbgPrint("==>LanceScanPci\n");
	#endif

	//
	// Read PCI vendor id and device id
	//
/* JK Start 
	if (NdisReadPciSlotInformation(
			Adapter->LanceMiniportHandle,
			Adapter->LanceSlotNumber,
			PCI_ID_REG,
			(PVOID)&Buffer,
			4) == 4)
	{
		#if DBG
		if(LanceDbg)
			DbgPrint("Pci Config Offset 0: %x \n",Buffer);
		#endif

		if(Buffer != PCNET_HL_PCI_ID)
		return;

	}
	else
	{
		return;
	}
JK END */

/************* JK start WinCE - Added the following loop to find which slot the PCI device is on ************/
	for ( i=0; i < 255 ; i++){  
		if (NdisReadPciSlotInformation(
			Adapter->LanceMiniportHandle,
			Adapter->LanceSlotNumber,
			PCI_ID_REG,
			(PVOID)&Buffer,
			4) == 4)
		{
			#if DBG
				//if(LanceDbg)
				//	DbgPrint("Pci Config Offset 0: %x \n",Buffer);
			#endif

		 
			if( Buffer == PCNET_HL_PCI_ID) // jk added
				break;					// jk added 
			Adapter->LanceSlotNumber++;
		}
	 
	} //end for 

	if( i==255){
		//JK here if the PCI device was not found in any of the Slots
#if DBG
		DbgPrint("PCI device not found in any Slot - initialization failed. Return FALSE. \n");
#endif
			return ; // Failed to find the PCI device
	}

/************JK end ***************/


	if (!(Adapter->PhysicalIoBaseAddress = LanceGetPciIoAddress (Adapter)))
	{
		return;
	}

	if (!(Adapter->LanceInterruptVector = LanceGetPciIntVector (Adapter)))
	{
		return;
	}

	Adapter->LanceDmaChannel = NO_DMA;

	//
	// Set chip and bus types
	//

	LanceInitAdapterType (Adapter);


	#if DBG
		if (LanceDbg)
			DbgPrint("<==LanceScanPci\n");
		#endif
}	/* End of function LanceScanPci() */

STATIC
VOID
LanceInitAdapterType (
	IN PLANCE_ADAPTER Adapter
	)
{
	ULONG PatchData = 0;
	ULONG Buffer;
	//jk USHORT Data;

	//
	// Write a zero to a read only register (offset 0 in PCI config
	// space) for bug fixing
	//
	WRITE_PCI_ID_ZERO(Adapter,&PatchData)

	//
	// Now read the command register.
	//
	if (NdisReadPciSlotInformation(
	Adapter->LanceMiniportHandle,
	Adapter->LanceSlotNumber,
	PCI_COMMAND_REG,
	(PVOID)&Buffer,
	4) == 4)
	{

		//
		// Reset the status reg (silicon patch)
		//
		Buffer |= 0xffff0000;
	}
	else
	{
		return;
	}

	//
	// Write a zero to a read only register (offset 0 in PCI config
	// space) for bug fixing
	//
	WRITE_PCI_ID_ZERO(Adapter,&PatchData)

#if DBG
	if(LanceDbg)
		DbgPrint("Write to Command reg: %x \n",Buffer);
#endif

	NdisWritePciSlotInformation(
	Adapter->LanceMiniportHandle,
	Adapter->LanceSlotNumber,
	PCI_COMMAND_REG,
	(PVOID)&Buffer,
	4);

	//
	// Write a zero to a read only register (offset 0 in PCI config
	// space) for bug fixing
	//
	WRITE_PCI_ID_ZERO(Adapter,&PatchData)

	//
	// Disable DMA bus master
	//
	// Now read the command register.
	//
	if (NdisReadPciSlotInformation(
			Adapter->LanceMiniportHandle,
			Adapter->LanceSlotNumber,
			PCI_COMMAND_REG,
			(PVOID)&Buffer,
			4) == 4)
	{
		Buffer |= 0x04;
	}

	//
	// Write a zero to a read only register (offset 0 in PCI config
	// space) for bug fixing
	//
	WRITE_PCI_ID_ZERO(Adapter,&PatchData)

#if DBG
	if(LanceDbg||LancePMDbg)
		DbgPrint("Write to Command reg: %x \n",Buffer);
#endif

	NdisWritePciSlotInformation(
		Adapter->LanceMiniportHandle,
		Adapter->LanceSlotNumber,
		PCI_COMMAND_REG,
		(PVOID)&Buffer,
		4);

	//
	// Write a zero to a read only register (offset 0 in PCI config
	// space) for bug fixing
	//
	WRITE_PCI_ID_ZERO(Adapter,&PatchData)


	//
	// Determine DeviceType using Rev ID.
	//

	if (NdisReadPciSlotInformation(
			Adapter->LanceMiniportHandle,
			Adapter->LanceSlotNumber,
			PCI_REV_ID_REG,
			(PVOID)&Buffer,
			4) == 4)
	{
		Buffer &= 0xff;

#if DBG
	if(LanceDbg || LanceHLDbg)
		DbgPrint("PCI REV ID REG : %x \n",Buffer);
#endif

		if (Buffer >= PCNET_HOME_B0_REV_ID)
		{
			Adapter->DeviceType = PCNET_HOME_B0;
		}
		else
		{
			Adapter->DeviceType = PCNET_HOME_A1;
		}

	}

	//
	// Write a zero to a read only register (offset 0 in PCI config
	// space) for bug fixing
	//
	WRITE_PCI_ID_ZERO(Adapter,&PatchData)

#ifdef NDIS50_PLUS
			if (LanceGetPCICapabilityID(Adapter) & PCI_CAPABILITY_ID_POWER_MANAGEMENT)
			{
				Adapter->FuncFlags |= CAP_POWER_MANAGEMENT;
			}

			#ifdef DBG
			if (LancePMDbg)
				{
					DbgPrint("Adapter Function Flags : %x\n",Adapter->FuncFlags);
					DbgPrint("Adapter Port : %x\n", Adapter->PhysicalIoBaseAddress);
					DbgPrint("Adapter Port : %x\n", Adapter->MappedIoBaseAddress);
				}
			#endif

#endif NDIS50_PLUS

}

STATIC
UCHAR
LanceGetPciIntVector (
	IN PLANCE_ADAPTER Adapter
	)
{

ULONG RetCode = 0;
	//
	// Get device interrupt line
	//
	if (NdisReadPciSlotInformation(
		Adapter->LanceMiniportHandle,
		Adapter->LanceSlotNumber,
		PCI_INTR_REG,
		(PVOID)&RetCode,
		4) == 4)
	{

#if DBG
		if(LanceDbg)
		DbgPrint("Interrupt Offset 3c: %x \n",RetCode);
#endif

	}
	return (UCHAR)(RetCode & 0xFF);
}

STATIC
ULONG
LanceGetPciIoAddress (
	IN PLANCE_ADAPTER Adapter
	)

{
	ULONG Buffer;
	ULONG PatchData = 0;

	//
	// Write a zero to a read only register (offset 0 in PCI config
	// space) for bug fixing
	//
	WRITE_PCI_ID_ZERO(Adapter,&PatchData)

	//
	// Check PCI command register I/O enbaled bit
	//
	if (NdisReadPciSlotInformation(
		Adapter->LanceMiniportHandle,
		Adapter->LanceSlotNumber,
		PCI_COMMAND_REG,
		(PVOID)&Buffer,
		4) == 4)
	{

		#if DBG
			if(LanceDbg)
			DbgPrint("Pci Com/Stat Offset 4: %x \n",Buffer);
		#endif

		//
		// Check that IO space access enabled.
		//
		if (!(Buffer & 0x01))
		{
			return 0;
		}
	}
	else
	{
		return 0;
	}

	//
	// Write a zero to a read only register (offset 0 in PCI config
	// space) for bug fixing
	//
	WRITE_PCI_ID_ZERO(Adapter,&PatchData)

	//
	// Get I/O base address
	//
	if (NdisReadPciSlotInformation(
		Adapter->LanceMiniportHandle,
		Adapter->LanceSlotNumber,
		PCI_BASE_IO_REG,
		(PVOID)&Buffer,
		4) == 4)
	{
		#if DBG
		if(LanceDbg)
			DbgPrint("IOBase Offset 10: %x \n",Buffer);
		#endif

		//
		// Get base io address
		//
		Buffer &= ~1;

	}
	else
	{
		return 0;
	}
	//
	// Write a zero to a read only register (offset 0 in PCI config
	// space) for bug fixing
	//
	WRITE_PCI_ID_ZERO(Adapter,&PatchData)

	//
	// Save base io address if it is valid
	//
	if (Buffer != 0 )
	{
		return Buffer;
	}
	return 0;

} /* End LanceGetPciIoAddress () */

#ifdef NDIS50_PLUS

STATIC
ULONG
LanceGetPCICapabilityID (
	IN PLANCE_ADAPTER Adapter
	)
/*
Description :
	Check PCI config space offset 06h bit 4 to check if NEW_CAP is set.
	If so, get PCI Capability IDs from PCI config space offset 40h.
		PCI_CAPABILITY_ID_POWER_MANAGEMENT	0x01
		PCI_CAPABILITY_ID_AGP					0x02 (we don't support this)
Return :
	Capability IDs (default 0)
*/	
{
	USHORT Buffer;
	PCI_CAPABILITIES_HEADER capHeader;	
	ULONG PatchData = 0;
	ULONG capID = 0;
	UCHAR nextCap = PCI_CAP_ID_REG;

	#ifdef DBG
		if (LancePMDbg)
		{
			DbgPrint("==>LanceGetPCICapabilityID\n");
		}
	#endif

	//
	// Write a zero to a read only register (offset 0 in PCI config
	// space) for bug fixing
	//
	WRITE_PCI_ID_ZERO(Adapter,&PatchData)

	//
	// Check PCI status register NEW_CAP bit
	//
	if (NdisReadPciSlotInformation(
		Adapter->LanceMiniportHandle,
		Adapter->LanceSlotNumber,
		PCI_STATUS_REG,
		(PVOID)&Buffer,
		sizeof(Buffer)) == sizeof(Buffer))
	{

		#if DBG
			if(LancePMDbg)
			DbgPrint("Pci Status Offset 6: %x \n",Buffer);
		#endif

		//
		// Check that NEW_CAP is set.
		//
		if (!(Buffer & NEW_CAP))
		{
			return 0;
		}
	}
	else
	{
		return 0;
	}
	
	// nextCap will be zero if there are no more capability entries
	while (nextCap) {
		//
		// Write a zero to a read only register (offset 0 in PCI config
		// space) for bug fixing
		//
		WRITE_PCI_ID_ZERO(Adapter,&PatchData)

		//
		// Get PCI Capability ID
		//
		if (NdisReadPciSlotInformation(
				Adapter->LanceMiniportHandle,
				Adapter->LanceSlotNumber,
				nextCap,
				(PVOID)&capHeader,
				sizeof(capHeader)) == sizeof(capHeader))
		{
			#if DBG
			if(LancePMDbg)
				DbgPrint("Capacity ID Offset 40: %x \n",capHeader.CapabilityID);
			#endif

			//
			// Get Capability ID
			//
			capID |= capHeader.CapabilityID;
			nextCap = capHeader.Next;				
		}
		else
		{
			return 0;
		}
	}
	//
	// Write a zero to a read only register (offset 0 in PCI config
	// space) for bug fixing
	//
	WRITE_PCI_ID_ZERO(Adapter,&PatchData)

	return capID;

} /* End LanceGetPCICapability () */

#endif //NDIS50_PLUS


STATIC
VOID
LancePciEnableDma(
	IN PLANCE_ADAPTER Adapter
	)

/*++

Routine Description:

	This routine eables PCI device DMA by setting PCI command
	register DMA bit

Arguments:

	Adapter - The adapter for the hardware.

Return Value:

	None.

--*/

{

	#if DBG
		if (LanceDbg)
		DbgPrint("==>LancePciEnableDma\n");
	#endif

	LanceSetPciDma(Adapter, TRUE);

	#if DBG
		if (LanceDbg)
		DbgPrint("<==LancePciEnableDma\n");
	#endif
}


STATIC
VOID
LancePciDisableDma(
	IN PLANCE_ADAPTER Adapter
	)

/*++

Routine Description:

	This routine disables PCI device DMA by clearing PCI command
	register DMA bit

Arguments:

	Adapter - The adapter for the hardware.

Return Value:

	None.

--*/

{

	#if DBG
		if (LanceDbg)
		DbgPrint("==>LancePciDisableDma\n");
	#endif

	LanceSetPciDma(Adapter, FALSE);

	#if DBG
		if (LanceDbg)
		DbgPrint("<==LancePciDisableDma\n");
	#endif
}


STATIC
VOID
LanceSetPciDma(
	IN PLANCE_ADAPTER Adapter,
	BOOLEAN EnablePciDma
	)

/*++

Routine Description:

	This routine set/clean pci command register DMA bit

Arguments:

	Adapter - The adapter for the hardware.

	EnablePciDma - Flag for operation.

Return Value:

	None.

--*/

{

	ULONG Buffer;
	ULONG PatchData = 0;
	#if DBG
		if (LanceDbg)
		DbgPrint("==>LanceSetPciDma\n");
	#endif

	//
	// Read the command and status Register.
	//
	if (NdisReadPciSlotInformation(
		Adapter->LanceMiniportHandle,
		Adapter->LanceSlotNumber,
		PCI_COMMAND_REG,
		(PVOID)(&Buffer),
		4) != 4) {

		return;

	}

	//
	// Write a zero to a read only register (offset 0 in PCI config
	// space) for bug fixing
	//
	WRITE_PCI_ID_ZERO(Adapter,&PatchData)

	if (EnablePciDma) {

		//
		// Enable DMA
		//
		Buffer = (Buffer | 0x4) & 0xffff;

	} else {

		//
		// Disable DMA
		//
		Buffer &= 0x0000fffb;

	}

	//
	// Write this value to the command register.
	//
	NdisWritePciSlotInformation(
		Adapter->LanceMiniportHandle,
		Adapter->LanceSlotNumber,
		PCI_COMMAND_REG,
		(PVOID)(&Buffer),
		4);

	//
	// Write a zero to a read only register (offset 0 in PCI config
	// space) for bug fixing
	//
	WRITE_PCI_ID_ZERO(Adapter,&PatchData)

	#if DBG
		if (LanceDbg)
		DbgPrint("<==LanceSetPciDma\n");
	#endif
}


VOID
LanceInit(
	IN PLANCE_ADAPTER Adapter,
	IN BOOLEAN FullReset
	)

/*++

Routine Description:

	This routine sets up the initial init of the driver.

Arguments:

	Adapter - The adapter data structure pointer

	FullReset - Full reset flag for programming all the registers

Return Value:

	None.

--*/

{
	#if DBG
	USHORT Data;
		if (LanceDbg || LancePMDbg)
		DbgPrint("==>LanceInit, Adapter->OpFlags = %x\n", Adapter->OpFlags);
	#endif

	/* Check if reset is allowed	*/
	if (Adapter->OpFlags & (RESET_IN_PROGRESS | RESET_PROHIBITED))
		return;

	//
	// Only for HomeLan
	//
	Adapter->HLAC.RcvStateValid = TRUE;
	
	/* Disable interrupts and release any pending interrupt sources	*/
	LanceDisableInterrupt(Adapter);

	#if DBG
		if (LancePMDbg)
		{
		LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, &Data);
		DbgPrint("DisableInterrupt OK. CSR0 %x\n",Data);
		}
	#endif

	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, 0xff04);

	/* Set reset flag for checking in isr. The isr will	*/
	/* not read CSR0 register for the chip interrupt when	*/
	/* programming other registers.	Note: PCI device can	*/
	/* share an interrupt so our isr can be called all the	*/
	/* time; and programming CSR has two steps and the value	*/
	/* set inside address register should not be changed	*/
	/* by the isr	*/
	Adapter->OpFlags |= RESET_IN_PROGRESS;

	if(Adapter->OpFlags & IN_LINK_TIMER)
	{
		NdisStallExecution(1);
	}

	/* First we make sure that the device is stopped and no	*/
	/* more interrupts come out.	Also some registers must be	*/
	/* programmed with CSR0 STOP bit set	*/
	LanceStopChip(Adapter);

	#if DBG
		if (LancePMDbg)
		{
		LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, &Data);
		DbgPrint("LanceStopChip OK. CSR0 %x\n",Data);
		}
	#endif

	/* Initialize registers and data structure	*/
	LanceSetupRegistersAndInit(Adapter, FullReset);

	#if DBG
		if (LancePMDbg)
		{
		LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, &Data);
		DbgPrint("LanceSetupReg OK. CSR0 %x\n",Data);
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
		LanceRegChkDbg = 0;
	#endif

	/* Start the chip	*/
	LanceStartChip(Adapter);
	#if DBG
		if (LanceDbg || LancePMDbg)
		{
		LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, &Data);
		DbgPrint("<==LanceInit. CSR0 %x\n",Data);
		}
	#endif
	Adapter->OpFlags &= ~RESET_IN_PROGRESS;
}


STATIC
VOID
LanceStopChip(
	IN PLANCE_ADAPTER Adapter
	)

/*++

Routine Description:

	This routine is used to stop the controller and disable
	controller's interrupt

Arguments:

	Adapter - The adapter data structure pointer

Return Value:

	None.

--*/

{

	USHORT Time; //jk , Data;
	UINT Timeout = START_STOP_TIMEOUT;

	#if DBG
		if (LanceDbg)
		DbgPrint("==>LanceStopChip\n");
	#endif

	//
	// Stop the chip
	//
	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, LANCE_CSR0_STOP);

	//
	// Ensure that the chip stops completely with interrupts disabled.
	//
	for (Time = 0; Time < 5; Time++)
		NdisStallExecution(1);

	#if DBG
		if (LanceDbg)
		DbgPrint("<==LanceStopChip\n");
	#endif
}


STATIC
VOID
LanceSetupRegistersAndInit(
	IN PLANCE_ADAPTER Adapter,
	IN BOOLEAN FullReset
	)

/*++

Routine Description:

	It is this routines responsibility to intialize transmit and
	receive descriptor rings but not start the chip.

Arguments:

	Adapter:	The adapter whose hardware is to be initialized.
	FullReset:	Full reset flag for programming all the registers

Return Value:

	None.

NOTES:

	LanceInitRxPacketPool *MUST* be called before this routine to set up 
	the NDIS packet array in the adapter structure. 

--*/
{

	UCHAR	i;
	USHORT	Data;

	//jk PLANCE_TRANSMIT_DESCRIPTOR		TransmitDescriptorRing;
	PLANCE_TRANSMIT_DESCRIPTOR_HI	TransmitDescriptorRingHi;
	NDIS_PHYSICAL_ADDRESS			TransmitBufferPointerPhysical;
	//jk PLANCE_RECEIVE_DESCRIPTOR		ReceiveDescriptorRing;
	PLANCE_RECEIVE_DESCRIPTOR_HI	ReceiveDescriptorRingHi;
	NDIS_PHYSICAL_ADDRESS			ReceiveBufferPointerPhysical;

	#if DBG
	if (LanceDbg)
		DbgPrint("==>LanceSetupRegistersAndInit\n");
	#endif
	/* Initialize the Rx/Tx descriptor ring structures	*/
	TransmitBufferPointerPhysical = Adapter->TransmitBufferPointerPhysical;
	ReceiveBufferPointerPhysical = Adapter->ReceiveBufferPointerPhysical;

	/* Set the Software Style to 32 Bits (PCNET-PCI).	*/
	TransmitDescriptorRingHi =
	(PLANCE_TRANSMIT_DESCRIPTOR_HI)Adapter->TransmitDescriptorRing;

	/* Set the software style to 32 Bits	*/

	LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR58, &Data);

	/* Mask IOStyle bits.	*/

	Data &= 0xff00 ;

	/* Enable SSIZE32	*/
	/* CSRPCNET + PCNET PCI II Sw style, will be set by the controller.	*/
	Data |= SW_STYLE_2 ;
	Adapter->SwStyle = SW_STYLE_2;

	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR58, Data);

	/* Initialize transmit descriptors	*/
	for (i = 0; i < TRANSMIT_BUFFERS; i++,TransmitDescriptorRingHi++)
	{
		//
		// Initialize transmit buffer pointer
		//
		TransmitDescriptorRingHi->LanceBufferPhysicalLow =
		LANCE_GET_LOW_PART_ADDRESS(NdisGetPhysicalAddressLow(
		TransmitBufferPointerPhysical) + (i * TRANSMIT_BUFFER_SIZE));

		TransmitDescriptorRingHi->LanceBufferPhysicalHighL =
		LANCE_GET_HIGH_PART_ADDRESS(NdisGetPhysicalAddressLow(
		TransmitBufferPointerPhysical) + (i * TRANSMIT_BUFFER_SIZE));

		TransmitDescriptorRingHi->LanceBufferPhysicalHighH =
		LANCE_GET_HIGH_PART_ADDRESS_H(NdisGetPhysicalAddressLow(
		TransmitBufferPointerPhysical) + (i * TRANSMIT_BUFFER_SIZE));

		TransmitDescriptorRingHi->ByteCount = (SHORT)0xF000;
		TransmitDescriptorRingHi->TransmitError = 0;

		// Set STP and ENP bits.
		TransmitDescriptorRingHi->LanceTMDFlags = (STP | ENP);
	} /* END "for" LOOP */

	ReceiveDescriptorRingHi =
	(PLANCE_RECEIVE_DESCRIPTOR_HI)Adapter->ReceiveDescriptorRing;

	/* Initialize receiving descriptors	*/
	for (i = 0; i < RECEIVE_BUFFERS; i++, ReceiveDescriptorRingHi++)
	{
		ReceiveDescriptorRingHi->LanceBufferPhysicalLow =
		LANCE_GET_LOW_PART_ADDRESS(NdisGetPhysicalAddressLow(
		ReceiveBufferPointerPhysical) + (i * RECEIVE_BUFFER_SIZE));

		ReceiveDescriptorRingHi->LanceBufferPhysicalHighL =
		LANCE_GET_HIGH_PART_ADDRESS(NdisGetPhysicalAddressLow(
		ReceiveBufferPointerPhysical) + (i * RECEIVE_BUFFER_SIZE));

		ReceiveDescriptorRingHi->LanceBufferPhysicalHighH =
		LANCE_GET_HIGH_PART_ADDRESS_H(NdisGetPhysicalAddressLow(
		ReceiveBufferPointerPhysical) + (i * RECEIVE_BUFFER_SIZE));

		/* Make Lance the owner of the descriptor	*/
		ReceiveDescriptorRingHi->LanceRMDFlags = OWN;
		ReceiveDescriptorRingHi->BufferSize = -RECEIVE_BUFFER_SIZE;
		ReceiveDescriptorRingHi->ByteCount = 0;
		ReceiveDescriptorRingHi->LanceRMDReserved1 = 0;
#ifdef NDIS40_PLUS
		/* Write the address of this descriptor's LanceRMDFlags member */
		/* to the MiniportReserved field of the relative NDIS packet */
		*((UCHAR **)&(Adapter->pNdisPacket[i]->MiniportReserved[0])) = &(ReceiveDescriptorRingHi->LanceRMDFlags);
#endif
	} /* END "for" LOOP */

	//
	// Reset Power Management STOP flag
	//

	Adapter->OpFlags &= RESET_MASK;

#ifdef NDIS40_PLUS
	Adapter->TxBufsUsed=0;	/* Reset outstanding tx buffer count */
#endif

	//
	// Initialize NextTransmitDescriptorIndex.
	//

	Adapter->NextTransmitDescriptorIndex = 0;

	//
	// Initialize TailTransmitDescriptorIndex.
	//

	Adapter->TailTransmitDescriptorIndex = 0;

	//
	// Initialize NextReceiveDescriptorIndex.
	//

	Adapter->NextReceiveDescriptorIndex = 0;

	//
	// Initialize Csr0Value to 0
	//

	Adapter->Csr0Value = 0;

	/* Global setting for csr4 register	*/
	LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR4, &Data);
	Data |= (LANCE_CSR4_AUTOPADTRANSMIT | LANCE_CSR4_DPOLL | 0x0004);
	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR4, Data);
	
	/* write dma burst and bus control register bcr18	*/
	LANCE_READ_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR18, &Data);
	Data |= (LANCE_BCR18_BREADE | LANCE_BCR18_BWRITE );
	LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR18, Data);
	LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR4, &Data);
	Data |= LANCE_CSR4_DMAPLUS;
	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR4, Data);
	/* Transmit Start Point setting(csr80)	*/
	LANCE_READ_CSR(Adapter->MappedIoBaseAddress, 80, &Data);
	Data |= 0x0800;
	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, 80, Data);

	LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR3, &Data);

//for HomeLan
	if (Adapter->HLBackOff)
		Data |= 0x0008;
	else
		Data &= ~0x0008;

	Data |= (LANCE_CSR3_IDONM | LANCE_CSR3_MERRM | LANCE_CSR3_DXSUFLO | LANCE_CSR3_BABLM);

	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR3, Data);

	/* Initialize CSR registers */

	if (FullReset)
	{
		//
		// Program LEDs
		//

		InitLEDs(Adapter);

		//
		// Program CSR1 and CSR2 with initialization block physical address
		//

		LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR2,
				LANCE_GET_HIGH_PART_PCI_ADDRESS(NdisGetPhysicalAddressLow(
				Adapter->InitializationBlockPhysical)));

		LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR1,
				LANCE_GET_LOW_PART_ADDRESS(NdisGetPhysicalAddressLow(
				Adapter->InitializationBlockPhysical)));
	}

#if DBG
	if (LanceDbg)
		DbgPrint("<==LanceSetupRegistersAndInit\n");
#endif
}



STATIC
VOID
InitLEDs (
	IN PLANCE_ADAPTER Adapter
	)
{
	ULONG *	pLEDs[4];
	USHORT	n;
	USHORT	Data;

#if DBG
	if (LanceDbg)
		DbgPrint("==>InitLEDs\n");
#endif
	pLEDs[0] = &(Adapter->led0);
	pLEDs[1] = &(Adapter->led1);
	pLEDs[2] = &(Adapter->led2);
	pLEDs[3] = &(Adapter->led3);

	LANCE_READ_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR2, &Data);
	LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR2, Data|LANCE_BCR2_LEDPE);

	for (n=0; n<4; n++)
	{
		if (*pLEDs[n] != LED_DEFAULT)
		{
			LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress,
			LANCE_LED0_STAT + n ,
			*pLEDs[n]);
		}
	}

#if DBG
	if (LanceDbg)
		DbgPrint("<==InitLEDs\n");
#endif

} /* End of function InitLEDs () */

STATIC
VOID
LanceStartChip(
	IN PLANCE_ADAPTER Adapter
	)

/*++

Routine Description:

	This routine loads initialization block and starts the chip.

Arguments:

	Adapter - The adapter data structure pointer

Return Value:

	None.

--*/

{

	UINT Data;
	UINT Timeout = START_STOP_TIMEOUT;
	#if DBG
		if (LanceDbg)
		DbgPrint("==>LanceStartChip\n");
	#endif

	//
	// Mask IDON
	//
	LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR3, &Data);
	Data |= LANCE_CSR3_IDONM;
	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR3, Data);

	/* Clear the IDON bit and other interrupt bits in CSR0 with	*/
	/* chip interrupt disabled	*/
	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, 0xFF00);

	/* Load initialization block into controller	*/
	LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, LANCE_CSR0_INIT);

	/* Waiting until IDON bit set	*/
	while (Timeout--) {

		/* Read CSR0	*/
		LANCE_READ_CSR(Adapter->MappedIoBaseAddress, LANCE_CSR0, &Data);

		/* Check if IDON bit set	*/
		if (Data & LANCE_CSR0_IDON) {

		/* Now clear reset flag to allow the isr reading	*/
		/* CSR0 for the interrupt	*/
		Adapter->OpFlags &= ~RESET_IN_PROGRESS;

		/* Clear all interrupt status bits and start chip	*/
		if (Adapter->OpFlags & IN_INTERRUPT_DPC) {

			LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, 
					LANCE_CSR0,
					LANCE_CSR0_START | LANCE_CSR0_INIT);

		} else {

			LANCE_WRITE_CSR(Adapter->MappedIoBaseAddress, 
					LANCE_CSR0,
					LANCE_CSR0_START | LANCE_CSR0_INIT | LANCE_CSR0_IENA);

		}
		
		//
		// Exit the loop
		//
		break;

		}

		//
		// Give some delay
		//
		NdisStallExecution(1);
	}

	#if DBG
		if (LanceDbg)
		DbgPrint("<==LanceStartChip, Timeout %d\n", Timeout);
	#endif
}


NDIS_STATUS
LanceReset(
	OUT PBOOLEAN AddressingReset,
	IN NDIS_HANDLE MiniportAdapterContext
	)

/*++

Routine Description:

	The LanceReset request instructs the miniport to issue a hardware reset
	to the network adapter.	The miniport also resets its software state.	See
	the description of MiniportReset for a detailed description of this request.

Arguments:

	AddressingReset - Set to TRUE if LanceSetInformation has to be called

	MiniportAdapterContext - The context value set by this mini-port.

Return Value:

	The function value is the status of the operation.

--*/

{

	//jk USHORT Data;

	//
	// Holds the status that should be returned to the caller.
	//
	PLANCE_ADAPTER Adapter = (PLANCE_ADAPTER)MiniportAdapterContext;

	#if DBG
		if (LancePMDbg)
		DbgPrint("==>LanceReset : Adapter->OpFlags = %x\n", Adapter->OpFlags);
	#endif

	if(Adapter->OpFlags & LANCE_RESET_CALLED)
	{
		return NDIS_STATUS_RESET_IN_PROGRESS;
	}

	Adapter->OpFlags |= LANCE_RESET_CALLED;

	if (Adapter->OpFlags & LANCE_DEFERRED_SEND_PROCESSING)
	{
		NdisStallExecution(40);
	#if DBG
		if (LanceTxQueueDbg)
		DbgPrint("Stall");
	#endif
		
	}

	ASSERT(!(Adapter->OpFlags & RESET_IN_PROGRESS));

	if (Adapter->OpFlags & INIT_COMPLETED) {

#ifdef NDIS50_PLUS
		if (Adapter->FuncFlags & CAP_POWER_MANAGEMENT)
		{
	 		LanceInitPMR(Adapter);
		}
#endif //NDIS50_PLUS

		//
		// Stop chip
		//
		LanceStopChip(Adapter);

		//
		// Do software reset
		//
		// NdisRawReadPortUshort(Adapter->MappedIoBaseAddress + LANCE_RESET_PORT, &Data);

		//
		// Clear operation flags for reset
		//
		Adapter->OpFlags &= ~(RESET_IN_PROGRESS | RESET_PROHIBITED);

		#if DBG
		if (LanceInitDbg)
			DbgPrint("LanceReset calls LanceInit.\n");
		#endif
		//
		// Restart the chip
		//
		LanceInit(Adapter, TRUE);

/* TxQueue */
//		NdisAcquireSpinLock(&Adapter->TxQueueLock);
	#if DBG
		if (LanceTxQueueDbg)
		DbgPrint(" %i\n",Adapter->NumPacketsQueued);
	#endif

		while(Adapter->FirstTxQueue)
		{
			PNDIS_PACKET QueuePacket = Adapter->FirstTxQueue;

			Adapter->NumPacketsQueued--;
			DequeuePacket(Adapter->FirstTxQueue, Adapter->LastTxQueue);
//      	NdisReleaseSpinLock(&Adapter->TxQueueLock);

			NdisMSendComplete(
					Adapter->LanceMiniportHandle,
					QueuePacket,
					NDIS_STATUS_RESET_IN_PROGRESS);

//			NdisAcquireSpinLock(&Adapter->TxQueueLock);
		}
//		NdisReleaseSpinLock(&Adapter->TxQueueLock);
		}

// for DMI
#ifdef DMI
	++Adapter->DmiSpecific[DMI_RESET_CNT];
#endif

	Adapter->OpFlags &= ~LANCE_RESET_CALLED;

	#if DBG
		if (LanceDbg)
		DbgPrint("<==LanceReset\n");
	#endif

#ifndef NDIS50_PLUS
	NdisMSendResourcesAvailable(Adapter->LanceMiniportHandle);
#endif

	return NDIS_STATUS_SUCCESS;

}


STATIC
VOID
LanceHalt(
	IN NDIS_HANDLE Context
	)

/*++

Routine Description:

	This routine stops the controller and release all the allocated
	resources.	Interrupts are enabled and no new request is sent to
	the driver.	It is not necessary to complete all outstanding
	requests before stopping the controller.

Arguments:

	Context - A pointer to the adapter data structure.

Return Value:

	None.

--*/

{

	PLANCE_ADAPTER Adapter = (PLANCE_ADAPTER)Context;
	USHORT Data;
	BOOLEAN IsCancelled;

	#if DBG
		if (LanceDbg)
		DbgPrint("==>LanceHalt\n");
	#endif
 
	NdisMCancelTimer (&(Adapter->CableTimer),&IsCancelled);

/* TxQueue */
	while(Adapter->FirstTxQueue)
	{
		PNDIS_PACKET QueuePacket = Adapter->FirstTxQueue;

		Adapter->NumPacketsQueued--;
		DequeuePacket(Adapter->FirstTxQueue, Adapter->LastTxQueue);

		NdisMSendComplete(
				Adapter->LanceMiniportHandle,
				QueuePacket,
				NDIS_STATUS_FAILURE);

	}
	NdisFreeSpinLock(&Adapter->TxQueueLock);

	//
	// Stop the controller and disable the controller interrupt
	//
	LanceStopChip (Adapter);

	//
	// Save Default Phy Address.
	//
	LanceDisableExtPhy(Adapter->MappedIoBaseAddress,(USHORT)Adapter->ActivePhyAddress);
	LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR33, 
						Adapter->DefaultPhyAddress | ANR0 | 0x8000);

	LANCE_READ_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR49, &Data);

	switch ((USHORT)Adapter->DefaultPhyAddress)
	{
		case SFEX_PHY_ADDRESS:
				Data = (Data&PHY_SEL_MASK)|PHY_SEL_SFEX;
				break;
		case HOMERUN_PHY_ADDRESS:
				Data = (Data&PHY_SEL_MASK)|PHY_SEL_HOMERUN;
				break;
		default:
				Data = (Data&PHY_SEL_MASK)|PHY_SEL_EXTERNAL;
				break;
	}

	LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR49, Data&(~PHY_SEL_LOCK));
	NdisRawReadPortUshort(Adapter->MappedIoBaseAddress + LANCE_RESET_PORT, &Data);

	//
	// Deregister interrupt number
	//
	NdisMDeregisterInterrupt (&(Adapter->LanceInterruptObject));

	//
	// Deregister io address range
	//
	NdisMDeregisterIoPortRange(
			Adapter->LanceMiniportHandle,
			Adapter->PhysicalIoBaseAddress,
			0x20,
			(PVOID)(Adapter->MappedIoBaseAddress)
			);

#ifdef NDIS40_PLUS
	LanceFreeRxPacketPool(Adapter);
#endif		

	//
	// Release allocated memory for initiation block, 
	// descriptors and buffers
	//
	LanceDeleteAdapterMemory (Adapter);

	//
	// Release allocated memory for driver data storage
	//
	LANCE_FREE_MEMORY(Adapter, sizeof(LANCE_ADAPTER));

	#if DBG
		if (LanceDbg)
		DbgPrint("<==LanceHalt\n");
	#endif
}


STATIC
NDIS_STATUS
LanceTransferData(
	OUT PNDIS_PACKET Packet,
	OUT PUINT BytesTransferred,
	IN NDIS_HANDLE MiniportAdapterContext,
	IN NDIS_HANDLE MiniportReceiveContext,
	IN UINT ByteOffset,
	IN UINT BytesToTransfer
	)

/*++

Routine Description:

	A protocol calls the LanceTransferData request (indirectly via
	NdisTransferData) from within its Receive event handler
	to instruct the MAC to copy the contents of the received packet
	a specified paqcket buffer.

Arguments:

	Status - Status of the operation.

	MiniportAdapterContext - The context value set by the Miniport.

	MiniportReceiveContext - The context value passed by the MAC on its call
	to NdisMEthIndicateReceive.

	ByteOffset - An unsigned integer specifying the offset within the
	received packet at which the copy is to begin.	If the entire packet
	is to be copied, ByteOffset must be zero.

	BytesToTransfer - An unsigned integer specifying the number of bytes
	to copy.	It is legal to transfer zero bytes; this has no effect.	If
	the sum of ByteOffset and BytesToTransfer is greater than the size
	of the received packet, then the remainder of the packet (starting from
	ByteOffset) is transferred, and the trailing portion of the receive
	buffer is not modified.

	Packet - A pointer to a descriptor for the packet storage into which
	the MAC is to copy the received packet.

	BytesTransfered - A pointer to an unsigned integer.	The MAC writes
	the actual number of bytes transferred into this location.	This value
	is not valid if the return status is STATUS_PENDING.

Return Value:

	The function value is the status of the operation.

--*/

{

	PLANCE_ADAPTER Adapter = (PLANCE_ADAPTER)MiniportAdapterContext;

	#if DBG
		if (LanceDbg)
		DbgPrint("==>LanceTransferData routine\n");
	#endif

	//
	// If reset flag set, return with reset status
	//
	if (Adapter->OpFlags & RESET_IN_PROGRESS) {

		#if DBG
		if (LanceDbg)
			DbgPrint("LanceTransferData routine: found reset and return.\n");
		#endif

		return NDIS_STATUS_RESET_IN_PROGRESS;

	}

	//
	// Copy packet data into data buffers provided by system
	//
	LanceMovePacket(Adapter,
				Packet,
				BytesToTransfer,
				(PCHAR)MiniportReceiveContext + ByteOffset,
				BytesTransferred,
				FALSE
				);
		
	#if DBG
		if (LanceDbg)
		DbgPrint("<==LanceTransferData routine\n");
	#endif

	return NDIS_STATUS_SUCCESS;	

}


VOID
LanceMovePacket(
	IN PLANCE_ADAPTER Adapter,
	PNDIS_PACKET Packet,
	IN UINT BytesToCopy,
	PCHAR Buffer,
	OUT PUINT BytesCopied,
	IN BOOLEAN CopyFromPacketToBuffer
	)

/*++

Routine Description:

	Copy from an ndis packet into a buffer while CopyFromPacketToBuffer
	flag set.	Otherwise copy from a buffer into an ndis packet buffer

Arguments:

	Adapter - The adapter data structure pointer.

	Packet - The packet pointer.

	BytesToCopy - The number of bytes to copy.

	Buffer - The buffer pointer.

	BytesCopied - The number of bytes actually copied.	Can be less then
	BytesToCopy if the packet is shorter than BytesToCopy.

	CopyFromPacketToBuffer - Copy direction

	AccessSram - Flag for SRAM / DRAM access

Return Value:

	None

--*/

{

	UINT NdisBufferCount;
	PNDIS_BUFFER CurrentBuffer;
	PVOID VirtualAddress;
	UINT CurrentLength;
	UINT LocalBytesCopied = 0;
	UINT AmountToMove;

	#if DBG
		if (LanceDbg)
		DbgPrint("==>LanceMovePacket\n");
	#endif

	//
	// Take care of boundary condition of zero length copy.
	//
	*BytesCopied = 0;
	if (!BytesToCopy) {

		#if DBG
		if (LanceDbg)
			DbgPrint("LanceMovePacket routine: number of bytes to move is 0.\n");
		#endif

		return;

	}

	//
	// Get the first buffer.
	//
	NdisQueryPacket(
		Packet,
		NULL,
		&NdisBufferCount,
		&CurrentBuffer,
		NULL
		);

	//
	// Could have a null packet.
	//
	if (!NdisBufferCount) {

		#if DBG
		if (LanceDbg)
			DbgPrint("LanceMovePacket routine: buffer count is 0.\n");
		#endif

		return;

	}

	NdisQueryBuffer(
		CurrentBuffer,
		&VirtualAddress,
		&CurrentLength
		);

	while (LocalBytesCopied < BytesToCopy) {

		if (!CurrentLength) {
		
		NdisGetNextBuffer(
			CurrentBuffer,
			&CurrentBuffer
			);

		//
		// We've reached the end of the packet.	We return
		// with what we've done so far. (Which must be shorter
		// than requested.
		//
		if (!CurrentBuffer) 
			break;

		NdisQueryBuffer(
			CurrentBuffer,
			&VirtualAddress,
			&CurrentLength
			);

		continue;

		}

		//
		// Copy the data.
		//
		AmountToMove = (CurrentLength <= (BytesToCopy - LocalBytesCopied)) ?
						CurrentLength : (BytesToCopy - LocalBytesCopied);


		if (CopyFromPacketToBuffer) 

		LANCE_MOVE_MEMORY(
				Buffer,
				VirtualAddress,
				AmountToMove
				);

		else 

		LANCE_MOVE_MEMORY(
				VirtualAddress,
				Buffer,
				AmountToMove
				);

		Buffer = (PCHAR)Buffer + AmountToMove;
		LocalBytesCopied += AmountToMove;
		CurrentLength -= AmountToMove;

	}

	*BytesCopied = LocalBytesCopied;

	#if DBG
		if (LanceDbg)
		DbgPrint("<==LanceMovePacket\n");
	#endif
}

//jk STATIC
VOID
LanceGetActiveMediaInfo (
	IN PLANCE_ADAPTER Adapter
	)
{

USHORT	MyAbility;
USHORT	LinkPartnerAbility;
USHORT	BitMask = 0x8000;

	/* Read MII register offset 4h */
	LANCE_READ_MII (Adapter->MappedIoBaseAddress, 
							(Adapter->ActivePhyAddress | MII_MY_ABILITY), &MyAbility);

	MyAbility &= ABILITY_MASK;
	MyAbility >>= 5;	/* Align LSB of the ability field at bit 0 */

	/* Read MII register offset 5h */
	LANCE_READ_MII (Adapter->MappedIoBaseAddress, 
							(Adapter->ActivePhyAddress | MII_LNK_PRTNR), &LinkPartnerAbility);

	LinkPartnerAbility &= ABILITY_MASK;
	LinkPartnerAbility >>= 5;	/* Align LSB of the ability field at bit 0 */
	MyAbility &= LinkPartnerAbility;	/* AND the two together */
											/* and save in MyAbility */

	if (MyAbility)
	{
		while (!(BitMask & MyAbility))
		{
			BitMask >>= 1;
		}
	}

	switch (BitMask)	/* The order of bit testing is deliberate and is defined */
	{					/* in IEEE 802.3 Supplement 802.3u/D5.3 Dated June 12, 1995 */
							/* annex 28B, specifically 28B.2 and 28B.3 */
		case 0x08:	/* 100BASE-TX FD */
				Adapter->FullDuplex = TRUE;
				Adapter->LineSpeed = 100;
				break;

		case 0x10:	/* 100BASE-T4 */
		case 0x04:	/* 100BASE-TX */
				Adapter->FullDuplex = FALSE;
				Adapter->LineSpeed = 100;
				break;

		case 0x02:	/* 10BASE-T FD */
				Adapter->FullDuplex = TRUE;
				Adapter->LineSpeed = 10;
				break;

//			case 0x01:	/* 10BASE-T */
		default:	/* 10BASE-T */
				Adapter->FullDuplex = FALSE;
				Adapter->LineSpeed = 10;
				break;

	}
}

#ifdef NDIS40_PLUS
STATIC
BOOLEAN
LanceInitRxPacketPool(
	IN PLANCE_ADAPTER Adapter
	)
/*
 ** LanceInitRxPacketPool
 *
 *  FILENAME:		lance.c
 *
 *  PARAMETERS:		Adapter -- Pointer to the adapter structure.
 *
 *  DESCRIPTION:	Creates the ornate framework required for NDIS 4 to manage
 *					a few received packets.
 *
 *  RETURNS:		TRUE if successful, else FALSE. Simple enough, eh?
 *
 */
{
	NDIS_STATUS 					Status;
	NDIS_HANDLE 					PktPoolHandle;
	NDIS_HANDLE 					BufPoolHandle;
	PNDIS_PACKET 					Packet;
	PNDIS_BUFFER					Buffer;
	PNDIS_PACKET_OOB_DATA			OobyDoobyData;
	PUCHAR							BufferVirtAddr;
	int								n;

 #if DBG
	if (LanceDbg)
	{
		DbgPrint("==>LanceInitRxPacketPool\n");
	}
 #endif
	for (n=0; n<RECEIVE_BUFFERS; n++)
	{
		Adapter->pNdisPacket[n] = NULL;
	}

    NdisAllocatePacketPool (&Status, &PktPoolHandle, RECEIVE_BUFFERS, PROT_RESERVED_AREA_SIZE);

	/* Later on , we may elect to reduce the requested number of buffers	*/
	/* if the first allocation attempt fails. For now, just match the		*/
	/* number of hardware rx descriptors									*/

	if (Status != NDIS_STATUS_SUCCESS)
	{
		return FALSE;
 #if DBG
		if (LanceDbg)
		{
			DbgPrint("NdisAllocatePacketPool () Failed.\n<==LanceInitRxPacketPool\n");
		}
 #endif
	}

    NdisAllocateBufferPool (&Status, &BufPoolHandle, RECEIVE_BUFFERS);
	if (Status != NDIS_STATUS_SUCCESS)
	{
 #if DBG
		if (LanceDbg)
		{
			DbgPrint("NdisAllocateBufferPool () Failed.\n<==LanceInitRxPacketPool\n");
		}
 #endif
		/* Free the packet pool before exiting. */
		NdisFreePacketPool (PktPoolHandle);
		return FALSE;
	}

	/* The buffers linked to the buffer pool are the same as the receive	*/
	/* buffers allocated for the Lance chip. There is one buffer per 		*/
	/* descriptor.															*/

	/* Get the virtual address of the start of rx buffer space */
	BufferVirtAddr = Adapter->ReceiveBufferPointer;

	for (n=0; n<RECEIVE_BUFFERS; n++)
	{
		/* First, allocate an NDIS packet descriptor*/
		NdisAllocatePacket (&Status, &Packet, PktPoolHandle);

		/* If that worked, allocate an NDIS buffer descriptor */
		if (Status == NDIS_STATUS_SUCCESS)
		{
			NdisAllocateBuffer (&Status, &Buffer, BufPoolHandle, BufferVirtAddr, RECEIVE_BUFFER_SIZE);
		}

		/* If either packet or buffer allocation calls fail, we'll end up here */
		if (Status != NDIS_STATUS_SUCCESS)
		{
			/* Free NDIS packet resources */
			LanceFreeNdisPkts (Adapter);

			/* Free the packet pool before exiting. */
			NdisFreePacketPool (PktPoolHandle);

			/* Free the buffer pool before exiting, too. */
			NdisFreeBufferPool (BufPoolHandle);

 #if DBG
			if (LanceDbg)
			{
				DbgPrint("NdisAllocatePacket() and/or NdisAllocateBuffer() Failed.\n<==LanceInitRxPacketPool\n");
			}
 #endif
			return FALSE;
		}

/* initialize the NDIS_PACKET_OOB_DATA block HeaderSize to 14  */

		OobyDoobyData = NDIS_OOB_DATA_FROM_PACKET (Packet);
		NdisZeroMemory (OobyDoobyData, sizeof(PNDIS_PACKET_OOB_DATA));
		OobyDoobyData->HeaderSize = 14;
		OobyDoobyData->SizeMediaSpecificInfo = 0;
		OobyDoobyData->MediaSpecificInformation = NULL;


	   NdisChainBufferAtFront(Packet, Buffer);
		Adapter->pNdisPacket[n] = Packet;
		Adapter->pNdisBuffer[n] = Buffer;
		BufferVirtAddr += RECEIVE_BUFFER_SIZE;
	}
	Adapter->NdisPktPoolHandle = PktPoolHandle;
	Adapter->NdisBufPoolHandle = BufPoolHandle;
 #if DBG
			if (LanceDbg)
			{
				DbgPrint("<==LanceInitRxPacketPool\n");
			}
 #endif
	return TRUE;

} /* End of function LanceInitRxPacketPool() */

STATIC
VOID
LanceFreeRxPacketPool(
	IN PLANCE_ADAPTER Adapter
	)
/*
 ** LanceFreeRxPacketPool
 *
 *  FILENAME:		lance.c
 *
 *  PARAMETERS:		Adapter -- Pointer to the adapter structure.
 *
 *  DESCRIPTION:	Creates the ornate framework required for NDIS 4 to manage
 *					a few received packets.
 *
 */
{
	/* Free NDIS packet resources */
	LanceFreeNdisPkts (Adapter);

	/* Free the packet pool before exiting. */
	if (Adapter->NdisPktPoolHandle)
		NdisFreePacketPool (Adapter->NdisPktPoolHandle);

	/* Free the buffer pool before exiting, too. */
	if (Adapter->NdisBufPoolHandle)
		NdisFreeBufferPool (Adapter->NdisBufPoolHandle);
}

STATIC
VOID
LanceFreeNdisPkts (
	IN PLANCE_ADAPTER Adapter
	)
/*
 ** LanceFreeNdisPkts
 *
 *  FILENAME:		lance.c
 *
 *  PARAMETERS:		Adapter --	A pointer to the adapter structure
 *
 *  DESCRIPTION:	Frees any and all packet descriptors allocated to us.
 *					If a given packet pointer is null, it is skipped.
 *
 *  RETURNS:		void
 *
 */
{
	USHORT 			n;
	PNDIS_PACKET	ThisPacket;
	PNDIS_BUFFER	ThisBuffer;

	for (n=0; n<RECEIVE_BUFFERS; n++)
	{
		ThisPacket = Adapter->pNdisPacket[n];

		if (ThisPacket != NULL)
		{
		    NdisUnchainBufferAtFront (ThisPacket, &ThisBuffer);

			if (ThisBuffer != NULL)
			{
				NdisFreeBuffer (ThisBuffer);
			}
			NdisFreePacket (ThisPacket);
			Adapter->pNdisPacket[n] = NULL;
		}
	}
} /* End of function LanceFreeNdisPkts () */

#endif	/* NDIS40_PLUS */

VOID
LanceShutdownHandler(
	IN PVOID a
	)
{
	PLANCE_ADAPTER Adapter = (PLANCE_ADAPTER)a;
	USHORT Data;
	BOOLEAN IsCancelled;

	#if DBG
		if (LanceDbg)
		DbgPrint("==>LanceShutdownHandler\n");
	#endif

	NdisMCancelTimer (&(Adapter->CableTimer),&IsCancelled);

	LanceStopChip(Adapter);

	//
	// Save Default Phy Address.
	//

	LanceDisableExtPhy(Adapter->MappedIoBaseAddress,(USHORT)Adapter->ActivePhyAddress);
	LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR33, 
						Adapter->DefaultPhyAddress | ANR0 | 0x8000);

	LANCE_READ_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR49, &Data);

	switch ((USHORT)Adapter->DefaultPhyAddress)
	{
		case SFEX_PHY_ADDRESS:
				Data = (Data&PHY_SEL_MASK)|PHY_SEL_SFEX;
				break;
		case HOMERUN_PHY_ADDRESS:
				Data = (Data&PHY_SEL_MASK)|PHY_SEL_HOMERUN;
				break;
		default:
				Data = (Data&PHY_SEL_MASK)|PHY_SEL_EXTERNAL;
				break;
	}

	LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR49, Data&(~PHY_SEL_LOCK));

	//
	// Do hardware reset
	//
	NdisRawReadPortUshort(Adapter->MappedIoBaseAddress + LANCE_RESET_PORT, &Data);

	#if DBG
	if (LanceDbg)
		DbgPrint("<==LanceShutdownHandler\n");
	#endif
	return;
}

#ifdef NDIS50_PLUS
VOID
LanceInitPMR(
	PLANCE_ADAPTER Adapter
	)
/*
	Description :
		Initialize PMR_MANAGER.
*/
{
	PLIST_ENTRY	pPattern = &(Adapter->pmManager.patternList);
	PLIST_ENTRY	pFree = &(Adapter->pmManager.freeList);
	PLIST_ENTRY	pList = &(Adapter->pmManager.pmBlockPool);
	PPM_BLOCK pPmBlock;
	ULONG	i;

	#ifdef DBG
		if (LancePMDbg)
		{
			DbgPrint("==>LanceInitPMR\n");
		}
	#endif

	InitializeListHead(pPattern);
	InitializeListHead(pFree);
	InitializeListHead(pList);

	NdisZeroMemory(&Adapter->pmManager.pmRAM, sizeof(PMR));

	for (i=0; i<MAX_BLOCKS; i++)
		InsertTailList(pList, &Adapter->pmManager.pmBlock[i].linkage);

	pPmBlock = (PPM_BLOCK)RemoveHeadList(pList);
	pPmBlock->startAddress = BASE_PM_START_ADDRESS;
	pPmBlock->blockSize = MAX_PMR_ADDRESS - BASE_PM_START_ADDRESS;
	
	InsertTailList(pFree, (PLIST_ENTRY)pPmBlock);
	Adapter->pmManager.totalPMRUsed = 0;
	Adapter->pmManager.patterns = 0;
	Adapter->pmManager.freeBlocks = 1;
}
#endif //NDIS50_PLUS


/* Handy code for detecting reentrancy problems */
/* NEVER use in free builds ! */
#if DBG
UCHAR LanceMutex (USHORT Flag)
{
UCHAR	OldFlag;
	_asm
	{
		mov		ax, Flag
		xchg	al, Mutex
		mov		OldFlag, al
	}
	if ((Flag == LOCK) && OldFlag)
		_asm	int 3;
	return OldFlag;
}
#endif

/**********************************************************************
				Functions for the Home Lan
**********************************************************************/

BOOLEAN
IsExtPhyExist(
	IN ULONG IoBaseAddress,
	IN USHORT PhyAddress
	)
/*
	All the functions using BCR49 is for Laguna 3 or later.
	Assumes that BCR49 is programed as External Phy has been selected.
*/
{
	USHORT Data1,Data2; //jk ,Data;

	#ifdef DBG
		if(LanceHLDbg)
			DbgPrint("==>IsExtPhyExist\n");
	#endif

	LANCE_READ_MII(IoBaseAddress,PhyAddress|MII_IEEE_ID,&Data1);
	LANCE_READ_MII(IoBaseAddress,PhyAddress|MII_IEEE_ID1,&Data2);

	#ifdef DBG
		if(LanceHLDbg)
			DbgPrint("IEEE ID %x, IEEE ID1 %x\n",Data1,Data2);
	#endif

	if (Data1 == 0xFFFF)
		return FALSE;
	
	if (Data1 | Data2)
		return TRUE;

	return FALSE;

}

VOID
LanceFindPhyList(
	PLANCE_ADAPTER Adapter
	)
/*
	Find all the Phys. The last Phy in the list should be HomeRun (Check LanceFindActivePhy)
	Current implementation supports only SFEX and HomeRun.
*/
{
	switch (Adapter->DefaultPhyAddress)
	{
		case HOMERUN_PHY_ADDRESS :
		case SFEX_PHY_ADDRESS :
			/* No External Phy found */
			Adapter->NumberOfPhys = 2;
			Adapter->PhyAddress[1] = HOMERUN_PHY_ADDRESS;		
			Adapter->PhyAddress[0] = SFEX_PHY_ADDRESS;
			break;

		default :
			if (IsExtPhyExist(Adapter->MappedIoBaseAddress,(USHORT)Adapter->DefaultPhyAddress))
			{
			 	Adapter->NumberOfPhys = 3;
			 	Adapter->PhyAddress[2] = HOMERUN_PHY_ADDRESS;
	 			Adapter->PhyAddress[0] = Adapter->DefaultPhyAddress;
		 		Adapter->PhyAddress[1] = SFEX_PHY_ADDRESS;
				break;
			}
			else
			{
				Adapter->NumberOfPhys = 2;
				Adapter->PhyAddress[1] = HOMERUN_PHY_ADDRESS;		
				Adapter->PhyAddress[0] = SFEX_PHY_ADDRESS;

				if (Adapter->PhySelected == PHY_SEL_EXTERNAL)
					Adapter->PhySelected = FIND_ACTIVE_PHY;

				break;
			}
	}		

	#ifdef DBG
		if(LanceHLDbg)
			DbgPrint("Number of Ext PHYs' : %d\n", Adapter->NumberOfPhys);
	#endif

}

VOID
LanceDisableExtPhy(
	IN ULONG IoBaseAddress,
	IN USHORT PhyAddress
	)
/*
	Isolate selected Ext Phy from MII, using ANR0_ISO.
*/
{
	USHORT Data;
	USHORT TempData;
	
	#ifdef DBG
		if(LanceHLDbg)
			DbgPrint("Disable Ext PHY : %x\n", PhyAddress);
	#endif

	LANCE_READ_MII (IoBaseAddress, (PhyAddress | ANR0), &Data);
	#ifdef DBG
		if(LanceHLDbg)
			DbgPrint("Data in ANR0, %x\n", Data);
	#endif

	Data |= ANR0_ISO;	
	LANCE_WRITE_MII (IoBaseAddress, (PhyAddress | ANR0), Data);
	LANCE_READ_MII (IoBaseAddress, (PhyAddress | ANR0), &TempData);

	#ifdef DBG
		if(LanceHLDbg)
			DbgPrint("Written Data %x, Read Data %x\n", Data,TempData);
	#endif
}

VOID
LanceEnableExtPhy(
	IN ULONG IoBaseAddress,
	IN USHORT PhyAddress
	)
/*
	Enable selected Ext Phy from MII, using ANR0_ISO.
*/
{
	USHORT Data;
	USHORT TempData;
	
	#ifdef DBG
		if(LanceHLDbg)
			DbgPrint("Enable Ext PHY : %x\n", PhyAddress);
	#endif

	LANCE_READ_MII (IoBaseAddress, (PhyAddress | ANR0), &Data);
	#ifdef DBG
		if(LanceHLDbg)
			DbgPrint("Data in ANR0, %x\n", Data);
	#endif

	Data &= ~ANR0_ISO; //0xFBFF;	
	LANCE_WRITE_MII (IoBaseAddress, (PhyAddress | ANR0), Data);
	LANCE_READ_MII (IoBaseAddress, (PhyAddress | ANR0), &TempData);

	#ifdef DBG
		if(LanceHLDbg)
			DbgPrint("Written Data %x, Read Data %x\n", Data,TempData);
	#endif

}

VOID
LanceIsolatePhys(
	PLANCE_ADAPTER Adapter
	)
{
	ULONG i;
	USHORT Data; //jk ,TempData;

	LANCE_READ_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR49, &Data);

	for (i=0; i<Adapter->NumberOfPhys; i++)
	{
		LanceDisableExtPhy(Adapter->MappedIoBaseAddress, (USHORT)Adapter->PhyAddress[i]);		
	}
}

VOID
LanceSetSeletedPhy(
	PLANCE_ADAPTER Adapter
	)
{
	//jk USHORT Data,TempData; 

	#ifdef DBG
		if(LanceHLDbg)
			DbgPrint("==>LanceSetSelectedPhy\n");
	#endif

	switch ((UCHAR)Adapter->PhySelected)
	{
		case PHY_SEL_SFEX :
				Adapter->ActivePhyAddress = SFEX_PHY_ADDRESS;
				break;

		case PHY_SEL_EXTERNAL :
				Adapter->ActivePhyAddress = Adapter->DefaultPhyAddress;
				break;

		case PHY_SEL_HOMERUN :
		default :
				Adapter->ActivePhyAddress = HOMERUN_PHY_ADDRESS;
				break;
	}

	#ifdef DBG
		if(LanceHLDbg)
			DbgPrint("<==LanceSetSelectPhy \n");
	#endif

}

VOID
EverestLinkOn(
	PLANCE_ADAPTER Adapter
	)
{
	USHORT Data;
	LANCE_READ_MII(Adapter->MappedIoBaseAddress, 
						HOMERUN_PHY_ADDRESS | 11, &Data);
	Data &= 0xefff;
	LANCE_WRITE_MII(Adapter->MappedIoBaseAddress, 
						HOMERUN_PHY_ADDRESS | 11, Data);
}

VOID
LanceFindActivePhy(
	PLANCE_ADAPTER Adapter
	)
{
	ULONG i;
#ifdef DBG
	ULONG j;
#endif
	USHORT TempValue;
	//jk USHORT DefaultValue;
	//jk USHORT Data,TempData;
	USHORT delayAfterBugFix = 4;

	#ifdef DBG
		if(LanceHLDbg)
			DbgPrint("==>LanceFindActivePhy\n");
	#endif

	for (i = 0; i < Adapter->NumberOfPhys; i++)
	{
		Adapter->ActivePhyAddress = Adapter->PhyAddress[i];

		if((Adapter->PhyAddress[i] == HOMERUN_PHY_ADDRESS)&&Adapter->RunBugFix)
		{
			EverestBugFix(Adapter);
			while(delayAfterBugFix--)
			{
				NdisStallExecution(500);
				#ifdef DBG
					if(LanceHLDbg)
					{
						LANCE_READ_MII(Adapter->MappedIoBaseAddress, 
							(Adapter->PhyAddress[i] | MII_STAT_REG), &TempValue);
						DbgPrint("%i Stall : %x\n",delayAfterBugFix,TempValue);
					}
				#endif
			}
		}

		LANCE_READ_MII(Adapter->MappedIoBaseAddress, 
							(Adapter->PhyAddress[i] | MII_STAT_REG), &TempValue);
		if(HomeLanPhyLinkStatus(Adapter->MappedIoBaseAddress,(USHORT)Adapter->PhyAddress[i]))
		{
			Adapter->ActivePhyAddress = Adapter->PhyAddress[i];
			#ifdef DBG
				if(LanceHLDbg)
					DbgPrint("Found Link Active Port : %x\n",Adapter->ActivePhyAddress);

			#endif
			break;
		}
	}

#ifdef DBG
	if(LanceHLDbg)
	{
	for (j = 0; j < Adapter->NumberOfPhys; j++)
	{
		LANCE_READ_MII(Adapter->MappedIoBaseAddress, 
							(Adapter->PhyAddress[j] | MII_STAT_REG), &TempValue);
	 	DbgPrint("MII_STAT_REG for %x : %x\n",Adapter->PhyAddress[j],TempValue);

		LANCE_READ_MII(Adapter->MappedIoBaseAddress, 
							(Adapter->PhyAddress[j] | MII_STAT_REG), &TempValue);
		DbgPrint("MII_STAT_REG for %x : %x\n",Adapter->PhyAddress[j],TempValue);
	}
	}
#endif //DBG

	//
	// No Link Active Port found.
	//
	if (i == Adapter->NumberOfPhys)
	{
//
// Since HomeRun Link is not working, Use HomeRun as default.
//
/*
		LANCE_READ_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR49, &Data);
		DefaultValue = (Data & PHY_SEL_DEFAULT_MASK) >> PHY_SEL_DEFAULT_BIT;

		switch (DefaultValue)
		{
			case PHY_SEL_SFEX :
					Adapter->ActivePhyAddress = SFEX_PHY_ADDRESS;
					break;

			case PHY_SEL_EXTERNAL :
					Adapter->ActivePhyAddress = Adapter->DefaultPhyAddress;
					break;

			case PHY_SEL_HOMERUN :
			default :
					Adapter->ActivePhyAddress = HOMERUN_PHY_ADDRESS;
					break;
		}
*/
		Adapter->ActivePhyAddress = HOMERUN_PHY_ADDRESS;

	}

	switch (Adapter->ActivePhyAddress)
	{
		case HOMERUN_PHY_ADDRESS:
					Adapter->PhySelected = PHY_SEL_HOMERUN;
					break;
		case SFEX_PHY_ADDRESS:
					Adapter->PhySelected = PHY_SEL_SFEX;
					break;
		default:
					Adapter->PhySelected = PHY_SEL_EXTERNAL;
					break;
	}

	#ifdef DBG
		if(LanceHLDbg)
			DbgPrint("ActivePhyAddress %x\n",Adapter->ActivePhyAddress);
	#endif
}

BOOLEAN
HomeLanPhyLinkStatus (
	IN ULONG IoBaseAddress,
	IN USHORT PhyAddress
	)
{
	USHORT	TempValue;

	/* 
		Read MII status register(reg 1). Read twice!!! That's the way
		you can get right value. However, previous version of driver read
		just once and works OK. Therefore, I am reading just once in this
		function. And read once before this function got called for HomeLan
		Phy Scan.
	*/
	LANCE_READ_MII(IoBaseAddress, (PhyAddress | MII_STAT_REG), &TempValue);

	if (TempValue == 0xFFFF)
	{
		return FALSE;
	}

	/* Test link status bit and exit FALSE if necessary */
	if (!(TempValue & LS0))
	{
		return FALSE;
	}
	return TRUE;
}

VOID
LanceHomeLanPortScan(
	IN PLANCE_ADAPTER Adapter
	)
/*
	Fill out the ActivePhyAddress field in the Adapter data block.
	First isolate all the PHYs from MII.
	Always make only one PHY connected to MII.
	For each PHY check Link status. If Link is on, use that port all the time.
	Sequence should be ExtPhy, Sfex, and then HomeRun.
	(Current Implementaion supports only Sfex and HomeRun).

	For Release 1.2, LinkPurseOffOnIsolatedPhys has been added.
*/
{
	USHORT Data; //jk ,TempData;
	
	LANCE_READ_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR49, &Data);

	if (Data & PHY_SEL_LOCK)
	{
		#if DBG
			if (LanceHLDbg)
 				DbgPrint("PHY_SEL was Locked.\n");
		#endif
	}

	/*
		Save Default Phy Address
	*/
	if (Adapter->DefaultPhyAddress == 0)
	{
		LANCE_READ_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR33, &Adapter->DefaultPhyAddress);
		Adapter->DefaultPhyAddress &= PHY_ADDRESS_MASK;
	}

	/*
		Select Ext Phy
	*/
	Data &= PHY_SEL_MASK;
	Data |= PHY_SEL_EXTERNAL;

	LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR49, Data | PHY_SEL_LOCK );

	/*
		Set DANAS bit in BCR32 to get correct PHY link.
	*/
	LANCE_READ_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR32, &Data);
	LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR32, Data|DANAS);
		
	/*
		Find all the PHYs.
	*/
	LanceFindPhyList(Adapter);

	/*
		Isolate all PHYs from MII.
	*/
	LanceIsolatePhys(Adapter);

	if ((UCHAR)Adapter->PhySelected >= FIND_ACTIVE_PHY)
		LanceFindActivePhy(Adapter);
	else
		LanceSetSeletedPhy(Adapter);

	LanceEnableExtPhy(Adapter->MappedIoBaseAddress, (USHORT)Adapter->ActivePhyAddress);		

	LinkPurseControl(Adapter);

	LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR33,
						(USHORT)Adapter->ActivePhyAddress | 0x8000);

	#if DBG
		if (LanceHLDbg)
		{
			LANCE_READ_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR33,
						&Data);
			DbgPrint("BCR33 : %x.\n",Data);
		}
	#endif

	LANCE_READ_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR49, &Data);

	switch (Adapter->ActivePhyAddress)
	{
		case HOMERUN_PHY_ADDRESS :
				LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR49, 
									(Data & PHY_SEL_MASK) | PHY_SEL_HOMERUN);
				break;
		case SFEX_PHY_ADDRESS :
				LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR49, 
									(Data & PHY_SEL_MASK) | PHY_SEL_SFEX);
				break;
		
		default :
				LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR49, 
									(Data & PHY_SEL_MASK) | PHY_SEL_EXTERNAL);
				break;
	}

	/*
		ReSet DANAS bit in BCR32 for Auto Nego.
	*/
	LANCE_READ_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR32, &Data);
	LANCE_WRITE_BCR(Adapter->MappedIoBaseAddress, LANCE_BCR32, Data&(~DANAS));
		
}

VOID
LanceHomeLanLinkMonitor(
	IN PVOID SystemSpecific1,
	IN PLANCE_ADAPTER Adapter,
	IN PVOID SystemSpecific2,
	IN PVOID SystemSpecific3
	)
{
	//jk BOOLEAN IsCancelled;
	//jk PLANCE_ADAPTER TempAdapter;
	
	#if DBG
		if (LanceDbg)
		DbgPrint("==>LanceHomeLanLinkMonitor\n");
	#endif

	if ((Adapter->OpFlags & PM_MODE) || (Adapter->OpFlags & RESET_IN_PROGRESS))
	{
#ifndef NDIS40_PLUS // NDIS30
		NdisMSetTimer(&(Adapter->CableTimer),HOMELAN_TIMEOUT);
#endif
	 	return;
	}

	if ((Adapter->isAutoConfigOn)&&
		 (Adapter->ActivePhyAddress == HOMERUN_PHY_ADDRESS))
	{
		HomeLanMonitor(SystemSpecific1,
							Adapter,
							SystemSpecific2,
							SystemSpecific3);
	}

	// Not supporting Link status for HomeRun Phy at this moment.
#ifdef NDIS40_PLUS
	if ((Adapter->ActivePhyAddress == HOMERUN_PHY_ADDRESS) && Adapter->HLLinkOff )
	{
		return;
	}
#endif //NDIS40

	if (Adapter->LinkActive) 
	{
		Adapter->LinkActive = FALSE;
		if (Adapter->CableDisconnected)
		{
			Adapter->CableDisconnected = FALSE;

#ifdef NDIS40_PLUS
			NdisMIndicateStatus (Adapter->LanceMiniportHandle,
								NDIS_STATUS_MEDIA_CONNECT,
								NULL,
								0
								);
#endif //NDIS40
			#if DBG
				if (LanceHLDbg)
				DbgPrint("Media Connected.\n");
			#endif
		}
#ifndef NDIS40_PLUS // NDIS30
		NdisMSetTimer(&(Adapter->CableTimer),HOMELAN_TIMEOUT);
#endif
		return;
	}

	Adapter->OpFlags |= IN_LINK_TIMER;

	if (HomeLanPhyLinkStatus (Adapter->MappedIoBaseAddress,(USHORT)Adapter->ActivePhyAddress))
	{		
		if (Adapter->CableDisconnected)
		{
			Adapter->CableDisconnected = FALSE;
#ifdef NDIS40_PLUS
			NdisMIndicateStatus (Adapter->LanceMiniportHandle,
								NDIS_STATUS_MEDIA_CONNECT,
								NULL,
								0
								);
#endif //NDIS40
			#if DBG
				if (LanceHLDbg)
				DbgPrint("Media Connected.\n");
			#endif
		}
	}
	else
	{

	#if DBG
		if (LanceHLDbg)
		DbgPrint("Link disconnected!!!\n");
	#endif

		if (!Adapter->CableDisconnected)
		{
			Adapter->CableDisconnected = TRUE;
#ifdef NDIS40_PLUS
			NdisMIndicateStatus (Adapter->LanceMiniportHandle,
								NDIS_STATUS_MEDIA_DISCONNECT,
								NULL,
								0
								);
#endif //NDIS40
			#if DBG
				if (LanceHLDbg)
				DbgPrint("Media DisConnected.\n");
			#endif
		}

#ifndef NDIS40_PLUS // NDIS30
/* TxQueue */
	#if DBG
		if (LanceTxQueueDbg)
		DbgPrint(" %i\n",Adapter->NumPacketsQueued);
	#endif

		while(Adapter->FirstTxQueue)
		{
			PNDIS_PACKET QueuePacket = Adapter->FirstTxQueue;

			Adapter->NumPacketsQueued--;
			DequeuePacket(Adapter->FirstTxQueue, Adapter->LastTxQueue);

			NdisMSendComplete(
					Adapter->LanceMiniportHandle,
					QueuePacket,
					NDIS_STATUS_FAILURE);
		}
#endif
	}

	Adapter->OpFlags &= ~IN_LINK_TIMER;

#ifndef NDIS40_PLUS // NDIS30
	NdisMSetTimer(&(Adapter->CableTimer),HOMELAN_TIMEOUT);
#endif

	#if DBG
		if (LanceDbg)
		DbgPrint("<==LanceHomeLanLinkMonitor\n");
	#endif
}

VOID
LanceResetPHY(
	PLANCE_ADAPTER Adapter
	)
{
	ULONG	i;
	ULONG Data;
	
	#ifdef DBG
		if(LanceHLDbg)
			DbgPrint("LanceResetPHY\n");
	#endif

	// Reset the external PHY.
	// Write Auto-Negotiation Reg 0: External PHY Reset
	LANCE_WRITE_MII(Adapter->MappedIoBaseAddress,
						(Adapter->ActivePhyAddress | ANR0), ANR0_PR);

	// 100 milliseconds sounds like enough time.
	for ( i = 0; i < 200; i++ )
	{	
		NdisStallExecution(50);
		LANCE_READ_MII(Adapter->MappedIoBaseAddress,
							(Adapter->ActivePhyAddress | ANR0), &Data);

		if (!(Data & ANR0_PR))
		{
			#ifdef DBG
				if(LanceHLDbg)
					DbgPrint("Phy reset success.\n");
			#endif
			break;
		}
   }

	#ifdef DBG
		if(LanceHLDbg && (i==200))
			DbgPrint("PHY reset fail.\n");
	#endif
	
}

VOID
InitHomeRunPort(
	IN PLANCE_ADAPTER Adapter
	)
{
//	USHORT	Data,i;
//	USHORT	TempData;

	#ifdef DBG
		if(LanceHLDbg)
			DbgPrint("==>InitHomeRunPort\n");
	#endif

	ProgramHomeRunState(Adapter, (USHORT)Adapter->HomeRunState);
	HomeRunPhySetting(Adapter);

	#ifdef DBG
		if(LanceHLDbg)
			DbgPrint("<==InitHomeRunPort\n");
	#endif

}

VOID
HomeLanExtPhyConfig(
	IN PLANCE_ADAPTER Adapter
	)
{
	ULONG IoBaseAddress = Adapter->MappedIoBaseAddress;
	ULONG ExtPhyMode = Adapter->ExtPhyMode;
	//jk USHORT TempData;
	USHORT Data;
	UCHAR	 TempByte;

	LANCE_READ_BCR(IoBaseAddress, LANCE_BCR2, &Data);
	LANCE_WRITE_BCR(IoBaseAddress, LANCE_BCR2, Data|LANCE_BCR2_ASEL);

	LANCE_READ_BCR(IoBaseAddress, LANCE_BCR32, &TempByte);

	#if DBG
		if (LanceExtPhyDbg)
		DbgPrint("BCR32 : %x\n",TempByte);
	#endif

	TempByte &= FORCED_PHY_MASK;
	TempByte |= (DANAS|XPHYRST);		/* Disable auto-neg. auto setup. */

	switch	(ExtPhyMode)
	{
		case 0:		// Auto-negotiate (Is this correct???)
			TempByte |= XPHYANE;	/* Enable auto-neg. */
			break;

		case 1:		// 100Mb H.D.
			TempByte |= XPHYSP;		/* Set the speed bit */
			break;

		case 2:		// 100Mb F.D.
			TempByte |= (XPHYSP | XPHYFD); /* Set speed and full duplex bits */
			break;

		case 3:		// 10Mb H.D.
			break;

		case 4:		// 10Mb F.D.
			TempByte |= XPHYFD;		/* Set the full duplex bit */
			break;

	}
	/* Write the new BCR32 value with DANAS Set */
	LANCE_WRITE_BCR(IoBaseAddress, LANCE_BCR32, TempByte);

	/* Clear the DANAS bit */
	TempByte &= ~DANAS;		/* Enable auto-neg. auto setup. */

	/* Write the new BCR32 value with DANAS Clear */
	LANCE_WRITE_BCR(IoBaseAddress, LANCE_BCR32, TempByte);

	/* Clear the Reset bit */
	TempByte &= ~XPHYRST;		/* Enable auto-neg. auto setup. */

	/* Write the new BCR32 value with DANAS Clear */
	LANCE_WRITE_BCR(IoBaseAddress, LANCE_BCR32, TempByte);

 #if DBG
	if (LanceExtPhyDbg) {
		LANCE_READ_BCR(IoBaseAddress, LANCE_BCR32, &TempByte);
		DbgPrint("BCR32 : %x\n",TempByte);
	}
 #endif

}

VOID
CheckSfexPhySpeed(
	IN PLANCE_ADAPTER Adapter
	)
{
	#ifdef DBG
		if(LanceHLDbg)
			DbgPrint("==>CheckSfexPhySpeed\n");
	#endif

	if ((Adapter->ExtPhyMode != 4) && 
		(Adapter->ActivePhyAddress == SFEX_PHY_ADDRESS))
		Adapter->ExtPhyMode = 3;

	#ifdef DBG
		if(LanceHLDbg)
			DbgPrint("<==CheckSfexPhySpeed\n");
	#endif
}

VOID
ProgramHomeRunState(
	IN PLANCE_ADAPTER Adapter,
	USHORT	HLState
	)
{
	USHORT Data;

	#ifdef DBG
		if(LanceDbg)
			DbgPrint("==>ProgramHomeRunState\n");
	#endif

	LANCE_READ_MII(Adapter->MappedIoBaseAddress,
						(Adapter->ActivePhyAddress | HR_PHY_CTRL_REG), &Data);

	Data &= AID_COMMAND_MASK;

	//
	//	Program HomeRun's Power Level and Speed.
	//
	switch (HLState)
	{
		case STATE_LPHS:
							Data |= C_LOW_POWER | C_HIGH_SPEED;
							break;

		case STATE_HPHS: 
							Data |= C_HIGH_POWER | C_HIGH_SPEED;
							break;

		case STATE_HPLS: 
							Data |= C_HIGH_POWER | C_LOW_SPEED;
							break;

		default:
							Data |= C_LOW_POWER | C_HIGH_SPEED;
							Adapter->HomeRunState = STATE_LPHS;
							break;
	}

	LANCE_WRITE_MII(Adapter->MappedIoBaseAddress,
						(Adapter->ActivePhyAddress | HR_PHY_CTRL_REG), Data);

	#ifdef DBG
		if(LanceDbg)
			DbgPrint("<==ProgramHomeRunState\n");
	#endif
}

VOID
HomeLanMonitor(
	IN PVOID	SystemSpecific1,
	IN PLANCE_ADAPTER Adapter,
	IN PVOID	SystemSpecific2,
	IN PVOID	SystemSpecific3
	)
{
	USHORT currentState;
	USHORT i;

//	#ifdef DBG
//		if(LanceDbg)
//			DbgPrint("==>HomeLanMonitor\n");
//	#endif

	currentState = 0;

	if (Adapter->SyncRate && Adapter->HLAC.RcvStateValid)
	{	
		if ((currentState = HomeLanSyncRate(Adapter)) == 0 )
		{
			Adapter->HLAC.numberOfCRCs = 0;
			Adapter->HLAC.CRCStates 	= 0;
			return;
		}
	}

	if (Adapter->HLAC.numberOfCRCs < Adapter->HLAC.MaxAllowedCRCs)
	{
		Adapter->HLAC.numberOfCRCs = 0;
		Adapter->HLAC.CRCStates 	= 0;
		return;
	}	

	// Get the current HomeLan State
	if (!currentState)
	{	LANCE_READ_MII(Adapter->MappedIoBaseAddress,
						(Adapter->ActivePhyAddress | HR_PHY_CTRL_REG), &currentState);
		currentState = (currentState>>HIGH_POWER_BIT) & 0x0003;
		switch (currentState) {						
			case 1 : //HPLS
					currentState = STATE_HPLS;
					break;
			case 2 : //LPHS									
					currentState = STATE_LPHS;
					break;
			case 3 : //HPHS
					currentState = STATE_HPHS;
					break;
			default :
					#ifdef DBG
					if (LanceDbg)
						DbgPrint("Current State is not acceptable\n");
					#endif
					currentState = STATE_LPHS;
					break;
		}
	}

	// Check if current State is equal to or greater than the last state of 
	// CRC packet.

	i = (USHORT)((Adapter->HLAC.numberOfCRCs-1)%MAX_NEIGHBOR_STORAGE)*STATE_BITS;
	i = (USHORT)(Adapter->HLAC.CRCStates >> i) & 0x0003;
	if ( i < currentState)
	{
	 	Adapter->HLAC.numberOfCRCs = 0;
	 	Adapter->HLAC.CRCStates 	= 0;
	 	return;
	}

	// if current State is HPLS, then there is no State to go
	if (currentState == STATE_HPLS)
	{
	 	Adapter->HLAC.numberOfCRCs = 0;
	 	Adapter->HLAC.CRCStates 	= 0;
		return;
	}

	ProgramHomeRunState(Adapter, currentState);

	// update the state history
	Adapter->HLAC.stateHistory = (Adapter->HLAC.stateHistory << STATE_BITS) | (ULONG)currentState;

	Adapter->HLAC.numberOfCRCs = 0;
	Adapter->HLAC.CRCStates 	= 0;

//	#ifdef DBG
//		if(LanceHLDbg)
//			DbgPrint("<==HomeLanMonitor");
//	#endif

}


USHORT
HomeLanSyncRate(
	PLANCE_ADAPTER Adapter
	)
/*
	To fix False Rate Change Bug in Tut solution.
	return 0 if No False Rate Detected.
	otherwise return current HomeRun State.
*/
{
	USHORT	currentState;
	USHORT	receivedState;

	LANCE_READ_MII(Adapter->MappedIoBaseAddress,
					(Adapter->ActivePhyAddress | HR_PHY_CTRL_REG), &currentState);

	LANCE_READ_MII(Adapter->MappedIoBaseAddress,
					(Adapter->ActivePhyAddress | HR_PHY_STATUS_REG), &receivedState);

	currentState = (currentState>>HIGH_POWER_BIT) & 0x0003;
	receivedState = (receivedState>>RECEIVED_SPEED_BIT) & 0x0003;

	switch (currentState) {						
		case 1 : //HPLS
				currentState = STATE_HPLS;
				break;
		case 2 : //LPHS									
				currentState = STATE_LPHS;
				break;
		case 3 : //HPHS
				currentState = STATE_HPHS;
				break;
		default :
				#ifdef DBG
				if (LanceDbg)
					DbgPrint("Current State is not acceptable\n");
				#endif
				currentState = STATE_LPLS;
				break;
	}

	switch (receivedState) {						
		case 1 : //LPHS
				receivedState = STATE_LPHS;
				break;
		case 2 : //HPLS									
				receivedState = STATE_HPLS;
				break;
		case 3 : //HPHS
				receivedState = STATE_HPHS;
				break;
		default :
				#ifdef DBG
				if (LanceDbg)
					DbgPrint("Received State is not acceptable\n");
				#endif
				receivedState = STATE_LPLS;
				break;
	}

	if (currentState != receivedState)
	{
		if (currentState > receivedState)
			ProgramHomeRunState(Adapter, receivedState);
		else
			ProgramHomeRunState(Adapter, currentState);
	}
	else if (currentState == STATE_LPLS)
	{
		currentState = STATE_LPHS;
		ProgramHomeRunState(Adapter, currentState);
	}
	else
		return currentState;

	return 0;			
}	

VOID
HomeRunPhySetting(
	PLANCE_ADAPTER Adapter
	)
/*
	Additional HomeRun Phy Setting, such as Link_Purse_Disable.
*/
{
	USHORT Data;

	//
	// Disable Link Pulse
	//
	switch (Adapter->HLLinkOff)
	{
		case 3 :
				LANCE_READ_MII(Adapter->MappedIoBaseAddress,
							(Adapter->ActivePhyAddress | HR_PHY_STATUS_REG), &Data);

				LANCE_WRITE_MII(Adapter->MappedIoBaseAddress,
							(Adapter->ActivePhyAddress | HR_PHY_STATUS_REG), Data|LINK_PULSE_DISABLE);
				break;
		case 2 :
				LANCE_READ_MII(Adapter->MappedIoBaseAddress,
							(Adapter->ActivePhyAddress | HR_ANALOG_TEST_REG), &Data);

				LANCE_WRITE_MII(Adapter->MappedIoBaseAddress,
							(Adapter->ActivePhyAddress | HR_ANALOG_TEST_REG), Data|FORCE_LINK_VALID);
				break;
	}
	
	if (Adapter->DeviceType >= PCNET_HOME_B0)
	{
		if (Adapter->AnalogTest)
			LANCE_WRITE_MII(Adapter->MappedIoBaseAddress,
						(HOMERUN_PHY_ADDRESS | 31), Adapter->AnalogTest);
		if (Adapter->LevelCtrl)
			LANCE_WRITE_MII(Adapter->MappedIoBaseAddress,
						(HOMERUN_PHY_ADDRESS | 30), Adapter->LevelCtrl);
		if (Adapter->HomeRun0)
			LANCE_WRITE_MII(Adapter->MappedIoBaseAddress,
						(HOMERUN_PHY_ADDRESS | (Adapter->HomeRun0 >> 16)), 
						(USHORT)(Adapter->HomeRun0 & 0x0000FFFF));
		if (Adapter->HomeRun1)
			LANCE_WRITE_MII(Adapter->MappedIoBaseAddress,
						(HOMERUN_PHY_ADDRESS | (Adapter->HomeRun1 >> 16)), 
						(USHORT)(Adapter->HomeRun1 & 0x0000FFFF));
	}

}

/* 
	Write 0x8784 to Reg 30 of HomeRun Phy.
	Read  Reg 25 of HomeRun Phy and Save it's high byte, say REG25.
	Write 0x0025 to Reg 16 of HomeRun Phy.
	Write (REG25+4)<<8|(REG25+8) to Reg 25.
*/
VOID
EverestBugFix(
	PLANCE_ADAPTER Adapter
	)
{
#define MAX_READ_REG25 4

#ifdef DBG
	USHORT Data; //jk i
#endif

//	USHORT REG25[MAX_READ_REG25];
	USHORT TempData;

	#ifdef DBG
		if(LanceHLDbg)
			DbgPrint("==>EverestBugFix\n");
	#endif

//  	LanceResetPHY(Adapter);
	
	/* Write 0xc000 to Reg 31(Amplifier Sense) of HomeRun Phy. */
	LANCE_WRITE_MII(Adapter->MappedIoBaseAddress,
					(HOMERUN_PHY_ADDRESS | 31), 0xc000);

	/* Write 0x1589 to Reg 30(Drive Levels) of HomeRun Phy. */
	LANCE_WRITE_MII(Adapter->MappedIoBaseAddress,
					(HOMERUN_PHY_ADDRESS | 30), Adapter->LevelCtrl);
//	LANCE_WRITE_MII(Adapter->MappedIoBaseAddress,
//					(HOMERUN_PHY_ADDRESS | 30), 0x8786);

#if 0
	/* Read Reg 25 of HomeRun Phy and Save it's high byte, say REG25.*/
	Data = 0;
	LANCE_READ_MII(Adapter->MappedIoBaseAddress,
						(HOMERUN_PHY_ADDRESS | 25), &Data);
	Data = Data >> 8;
	i = 2;
	while (i--)
		NdisStallExecution(500);
		
	if (Data < 0x2d)
		Data = 0x2d;

	#ifdef DBG
		if(LanceHLDbg)
			DbgPrint("REG25 %x\n",Data);
	#endif

	//
	// write anr23 with the following value
	//
	LANCE_WRITE_MII(Adapter->MappedIoBaseAddress,
					(HOMERUN_PHY_ADDRESS | 23), ((Data+1)<<8)|0x0001);

	LANCE_READ_MII(Adapter->MappedIoBaseAddress,
					(HOMERUN_PHY_ADDRESS | 23), &TempData);

	i = 2;
	while (i--)
		NdisStallExecution(500);
	
	LANCE_WRITE_MII(Adapter->MappedIoBaseAddress,
					(HOMERUN_PHY_ADDRESS | 23), ((Data+1)<<8)|0x00ff);

	LANCE_READ_MII(Adapter->MappedIoBaseAddress,
					(HOMERUN_PHY_ADDRESS | 23), &TempData);

	i = 2;
	while (i--)
		NdisStallExecution(500);

	//
	// read current Noise Floor
	//	
	Data = 0;
	LANCE_READ_MII(Adapter->MappedIoBaseAddress,
						(HOMERUN_PHY_ADDRESS | 25), &Data);
	Data = Data >> 8;
//	NdisStallExecution(500);
#endif //0

	/* Write 0x0025 to Reg 16 of HomeRun Phy. */
	LANCE_READ_MII(Adapter->MappedIoBaseAddress,
					(HOMERUN_PHY_ADDRESS | 16), &TempData);
	LANCE_WRITE_MII(Adapter->MappedIoBaseAddress,
					(HOMERUN_PHY_ADDRESS | 16), TempData | 0x0020);

	/* Write (REG25+4)<<8|(REG25+8) to Reg 25.*/
//	LANCE_WRITE_MII(Adapter->MappedIoBaseAddress,
//					(HOMERUN_PHY_ADDRESS | 25), (Data+3)<<8);
	LANCE_WRITE_MII(Adapter->MappedIoBaseAddress,
					(HOMERUN_PHY_ADDRESS | 25), 0x3400);
	

	#ifdef DBG
		if(LanceHLDbg)
		{
			LANCE_READ_MII(Adapter->MappedIoBaseAddress,
						(HOMERUN_PHY_ADDRESS | 25), &Data);
			DbgPrint("REG25 %x\n",Data);
			LANCE_READ_MII(Adapter->MappedIoBaseAddress,
						(HOMERUN_PHY_ADDRESS | 30), &Data);
			DbgPrint("REG30 %x\n",Data);
			LANCE_READ_MII(Adapter->MappedIoBaseAddress,
						(HOMERUN_PHY_ADDRESS | 16), &Data);
			DbgPrint("REG16 %x\n",Data);
		}
	#endif

	#ifdef DBG
		if(LanceHLDbg)
			DbgPrint("<==EverestBugFix\n");
	#endif

}


VOID
LinkPurseControl(
	IN PLANCE_ADAPTER Adapter
	)
{
	USHORT i,Data;

	for (i=0; i<Adapter->NumberOfPhys; i++)
	{
		if (Adapter->ActivePhyAddress == Adapter->PhyAddress[i])
		{
			// Enable Link_Pulse if this Phy is active.
			LANCE_READ_MII(Adapter->MappedIoBaseAddress,
						(Adapter->PhyAddress[i] | HR_PHY_STATUS_REG), &Data);
			Data &= ~LINK_PULSE_DISABLE;
			LANCE_WRITE_MII(Adapter->MappedIoBaseAddress,
						(Adapter->PhyAddress[i] | HR_PHY_STATUS_REG), Data);
			continue;
		}

		LANCE_READ_MII(Adapter->MappedIoBaseAddress,
					(Adapter->PhyAddress[i] | HR_PHY_STATUS_REG), &Data);

		LANCE_WRITE_MII(Adapter->MappedIoBaseAddress,
					(Adapter->PhyAddress[i] | HR_PHY_STATUS_REG), Data|LINK_PULSE_DISABLE);
	}

}


VOID
LanceDeferredProcess(
	IN PVOID SystemSpecific1,
	IN PLANCE_ADAPTER Adapter,
	IN PVOID SystemSpecific2,
	IN PVOID SystemSpecific3
	)
{
	if(!(Adapter->OpFlags & LANCE_DEFERRED_SEND_PROCESSING))
		LanceDeferredSend(Adapter);
}

