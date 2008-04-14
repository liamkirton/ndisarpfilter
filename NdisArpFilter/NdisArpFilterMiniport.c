// ========================================================================================================================
// NdisArpFilter
//
// Copyright ©2007 Liam Kirton <liam@int3.ws>
// ========================================================================================================================
// NdisArpFilterMiniport.c
//
// Created: 02/07/2007
// ========================================================================================================================

#include <ndis.h>

#include "NdisArpFilter.h"
#include "NdisArpFilterMiniport.h"
#include "NdisArpFilterProtocol.h"

// ========================================================================================================================

VOID MiniportAdapterShutdown(IN PVOID ShutdownContext)
{
	//DbgPrint("<NdisArpFilter> -> MiniportAdapterShutdown()\n");
}

// ========================================================================================================================

VOID MiniportCancelSendPackets(IN NDIS_HANDLE MiniportAdapterContext, IN PVOID CancelId)
{
	ADAPTER *pAdapter = (ADAPTER *)MiniportAdapterContext;

	//DbgPrint("<NdisArpFilter> -> MiniportCancelSendPackets()\n");

	NdisCancelSendPackets(pAdapter->BindingHandle, CancelId);
}

// ========================================================================================================================

NDIS_STATUS MiniportInitialize(OUT PNDIS_STATUS OpenErrorStatus,
							   OUT PUINT SelectedMediumIndex,
							   IN PNDIS_MEDIUM MediumArray,
							   IN UINT MediumArraySize,
							   IN NDIS_HANDLE MiniportHandle,
							   IN NDIS_HANDLE WrapperConfigurationContext)
{
	NDIS_STATUS ndisStatus = NDIS_STATUS_SUCCESS;

	ADAPTER *pAdapter = NdisIMGetDeviceContext(MiniportHandle);
	UINT i = 0;
	
	//DbgPrint("<NdisArpFilter> -> MiniportInitialize()\n");

	do
	{
		pAdapter->MiniportHandle = MiniportHandle;

		//
		// Set Miniport Attributes
		//
		NdisMSetAttributesEx(MiniportHandle,
							 pAdapter,
							 0,
							 NDIS_ATTRIBUTE_IGNORE_PACKET_TIMEOUT |
							 NDIS_ATTRIBUTE_IGNORE_REQUEST_TIMEOUT |
							 NDIS_ATTRIBUTE_INTERMEDIATE_DRIVER |
							 NDIS_ATTRIBUTE_DESERIALIZE |
							 NDIS_ATTRIBUTE_NO_HALT_ON_SUSPEND,
							 0);

		//
		// Set Adapter Medium
		//
		for(i = 0; i < MediumArraySize; ++i)
		{
			if(MediumArray[i] == pAdapter->AdapterMedium)
			{
				*SelectedMediumIndex = i;
				break;
			}
		}

		pAdapter->MiniportDevicePowerState = NdisDeviceStateD0;

		//
		// Add Adapter Instance To Global Adapter List
		//
		NdisAcquireSpinLock(&g_Lock);
		InsertHeadList(&g_AdapterList, &pAdapter->Link);
		NdisReleaseSpinLock(&g_Lock);

		//
		// Register Device Object, If First Miniport
		//
		RegisterDevice();
	}
	while(FALSE);

	//DbgPrint("<NdisArpFilter> <- MiniportInitialize() [%x]\n", ndisStatus);

	*OpenErrorStatus = ndisStatus;
	return ndisStatus;
}

// ========================================================================================================================

VOID MiniportHalt(IN NDIS_HANDLE MiniportAdapterContext)
{
	NDIS_STATUS ndisStatus = NDIS_STATUS_SUCCESS;

	ADAPTER *pAdapter = (ADAPTER *)MiniportAdapterContext;
	PLIST_ENTRY i;

	//DbgPrint("<NdisArpFilter> -> MiniportHalt()\n");

	//
	// Remove Adapter Instance From Global Adapter List
	//
	NdisAcquireSpinLock(&g_Lock);
	for(i = g_AdapterList.Flink; i != &g_AdapterList; i = i->Flink)
	{
		ADAPTER *pAdapterIter = CONTAINING_RECORD(i, ADAPTER, Link);
		if(pAdapter == pAdapterIter)
		{
			RemoveEntryList(i);
			break;
		}
	}
	NdisReleaseSpinLock(&g_Lock);

	//
	// Deregister Device Object, If Last Miniport
	//
	DeregisterDevice();

	//
	// Close Adapter Handle
	//
	if(pAdapter->BindingHandle != NULL)
	{
		NdisResetEvent(&pAdapter->CompletionEvent);

		NdisCloseAdapter(&ndisStatus, pAdapter->BindingHandle);
		if(ndisStatus == NDIS_STATUS_PENDING)
		{
			NdisWaitEvent(&pAdapter->CompletionEvent, 0);
			ndisStatus = pAdapter->CompletionStatus;
		}

		if(ndisStatus != NDIS_STATUS_SUCCESS)
		{
			//DbgPrint("<NdisArpFilter> !! NdisCloseAdapter() Failed [%x].\n", ndisStatus);
		}
	}

	if(pAdapter->RecvPacketPoolHandle != NULL)
	{
		NdisFreePacketPool(pAdapter->RecvPacketPoolHandle);
		pAdapter->RecvPacketPoolHandle = NULL;
	}
	if(pAdapter->SendPacketPoolHandle != NULL)
	{
		NdisFreePacketPool(pAdapter->SendPacketPoolHandle);
		pAdapter->SendPacketPoolHandle = NULL;
	}

	NdisFreeSpinLock(&pAdapter->AdapterLock);
	NdisFreeMemory(pAdapter, 0, 0);

	//DbgPrint("<NdisArpFilter> <- MiniportHalt()\n");
}

// ========================================================================================================================

VOID MiniportPnPEventNotify(IN NDIS_HANDLE MiniportAdapterContext,
							IN NDIS_DEVICE_PNP_EVENT PnPEvent,
							IN PVOID InformationBuffer,
							IN ULONG InformationBufferLength)
{
	//DbgPrint("<NdisArpFilter> -> MiniportPnPEventNotify()\n");
}

// ========================================================================================================================

NDIS_STATUS MiniportQueryInformation(IN NDIS_HANDLE MiniportAdapterContext,
									 IN NDIS_OID Oid,
									 IN PVOID InformationBuffer,
									 IN ULONG InformationBufferLength,
									 OUT PULONG BytesWritten,
									 OUT PULONG BytesNeeded)
{
	NDIS_STATUS ndisStatus = NDIS_STATUS_FAILURE;

	ADAPTER *pAdapter = (ADAPTER *)MiniportAdapterContext;

	//DbgPrint("<NdisArpFilter> -> MiniportQueryInformation(%x)\n", Oid);

	do
	{
		if(Oid == OID_PNP_QUERY_POWER)
		{
			ndisStatus = NDIS_STATUS_SUCCESS;
			break;
		}
		else if(Oid == OID_GEN_SUPPORTED_GUIDS)
		{
			ndisStatus = NDIS_STATUS_NOT_SUPPORTED;
			break;
		}
		else if(pAdapter->MiniportDevicePowerState > NdisDeviceStateD0)
		{
			ndisStatus = NDIS_STATUS_FAILURE;
			break;
		}

		//
		// Pass Query To Lower Bound Miniport
		//
		pAdapter->Request.RequestType = NdisRequestQueryInformation;
		pAdapter->Request.DATA.QUERY_INFORMATION.Oid = Oid;
		pAdapter->Request.DATA.QUERY_INFORMATION.InformationBuffer = InformationBuffer;
		pAdapter->Request.DATA.QUERY_INFORMATION.InformationBufferLength = InformationBufferLength;
		pAdapter->RequestBytesNeeded = BytesNeeded;
		pAdapter->RequestBytesReadOrWritten = BytesWritten;

		NdisRequest(&ndisStatus,
					pAdapter->BindingHandle,
					&pAdapter->Request);
		if(ndisStatus != NDIS_STATUS_PENDING)
		{
			ProtocolRequestComplete(pAdapter->BindingHandle, &pAdapter->Request, ndisStatus);
			ndisStatus = NDIS_STATUS_PENDING;
		}
	}
	while(FALSE);

	//DbgPrint("<NdisArpFilter> <- MiniportQueryInformation() [%x]\n", ndisStatus);

	return ndisStatus;
}

// ========================================================================================================================

VOID MiniportReturnPacket(IN NDIS_HANDLE MiniportAdapterContext, IN PNDIS_PACKET Packet)
{
	ADAPTER *pAdapter = (ADAPTER *)MiniportAdapterContext;

	//DbgPrint("<NdisArpFilter> -> MiniportReturnPacket()\n");

	if(NdisGetPoolFromPacket(Packet) != pAdapter->RecvPacketPoolHandle)
	{
		NdisReturnPackets(&Packet, 1);
	}
	else
	{
		PNDIS_PACKET pRetPacket;
		RECV_RSVD *pRecvRsvd;

		pRecvRsvd = (RECV_RSVD *)Packet->MiniportReserved;
		pRetPacket = pRecvRsvd->Original;

		NdisFreePacket(Packet);
		NdisReturnPackets(&pRetPacket, 1);
	}
}

// ========================================================================================================================

VOID MiniportSendPackets(IN NDIS_HANDLE MiniportAdapterContext, IN PPNDIS_PACKET PacketArray, IN UINT NumberOfPackets)
{
	ADAPTER *pAdapter = (ADAPTER *)MiniportAdapterContext;

	NDIS_STATUS ndisStatus = NDIS_STATUS_SUCCESS;
	UINT i = 0;

	//DbgPrint("<NdisArpFilter> -> MiniportSendPackets(%d)\n", NumberOfPackets);

	for(i = 0; i < NumberOfPackets; ++i)
	{
		PNDIS_PACKET pPacket = PacketArray[i];
		PNDIS_PACKET pSendPacket = NULL;
		PNDIS_PACKET_STACK pPacketStack = NULL;
		PVOID packetMediaSpecificInfo = NULL;
		UINT packetMediaSpecificInfoSize = 0;

		BOOLEAN Remaining;

		if(pAdapter->MiniportDevicePowerState > NdisDeviceStateD0)
		{
			NdisMSendComplete(pAdapter->MiniportHandle, pPacket, NDIS_STATUS_FAILURE);
			continue;
		}

		pPacketStack = NdisIMGetCurrentPacketStack(pPacket, &Remaining);
		if(Remaining)
		{
			if(pAdapter->ProtocolDevicePowerState > NdisDeviceStateD0)
			{
				NdisMSendComplete(pAdapter->MiniportHandle, pPacket, NDIS_STATUS_FAILURE);
			}
			else
			{
				NdisSend(&ndisStatus, pAdapter->BindingHandle, pPacket);
				if(ndisStatus != NDIS_STATUS_PENDING)
				{
					NdisMSendComplete(pAdapter->MiniportHandle, pPacket, ndisStatus);
				}
			}
			continue;
		}

		do
		{
			if(pAdapter->ProtocolDevicePowerState > NdisDeviceStateD0)
			{
				ndisStatus = NDIS_STATUS_FAILURE;
				break;
			}

			NdisAllocatePacket(&ndisStatus, &pSendPacket, pAdapter->SendPacketPoolHandle);
			if(ndisStatus == NDIS_STATUS_SUCCESS)
			{
				SEND_RSVD *pSendRsvd;
				pSendRsvd = (SEND_RSVD *)pSendPacket->ProtocolReserved;
				pSendRsvd->Original = pPacket;

				NdisGetPacketFlags(pSendPacket) = NdisGetPacketFlags(pPacket);
				NDIS_PACKET_FIRST_NDIS_BUFFER(pSendPacket) = NDIS_PACKET_FIRST_NDIS_BUFFER(pPacket);
				NDIS_PACKET_LAST_NDIS_BUFFER(pSendPacket) = NDIS_PACKET_LAST_NDIS_BUFFER(pPacket);

				NdisMoveMemory(NDIS_OOB_DATA_FROM_PACKET(pSendPacket),
							   NDIS_OOB_DATA_FROM_PACKET(pPacket),
							   sizeof(NDIS_PACKET_OOB_DATA));

				NdisIMCopySendPerPacketInfo(pSendPacket, pPacket);

				NDIS_GET_PACKET_MEDIA_SPECIFIC_INFO(pPacket,
													&packetMediaSpecificInfo,
													&packetMediaSpecificInfoSize);

				if((packetMediaSpecificInfo != NULL) || (packetMediaSpecificInfoSize != 0))
				{
					NDIS_SET_PACKET_MEDIA_SPECIFIC_INFO(pSendPacket,
														packetMediaSpecificInfo,
														packetMediaSpecificInfoSize);
				}

				NdisSend(&ndisStatus, pAdapter->BindingHandle, pSendPacket);
				if(ndisStatus != NDIS_STATUS_PENDING)
				{
					NdisIMCopySendCompletePerPacketInfo(pPacket, pSendPacket);
					NdisFreePacket(pSendPacket);
				}
			}
		}
		while(FALSE);

		if(ndisStatus != NDIS_STATUS_PENDING)
		{
			NdisMSendComplete(pAdapter->MiniportHandle, pPacket, ndisStatus);
		}
	}

	//DbgPrint("<NdisArpFilter> <- MiniportSendPackets(%d)\n", NumberOfPackets);
}

// ========================================================================================================================

NDIS_STATUS MiniportSetInformation(IN NDIS_HANDLE MiniportAdapterContext,
								   IN NDIS_OID Oid,
								   IN PVOID InformationBuffer,
								   IN ULONG InformationBufferLength,
								   OUT PULONG BytesRead,
								   OUT PULONG BytesNeeded)
{
	NDIS_STATUS ndisStatus = NDIS_STATUS_FAILURE;

	ADAPTER *pAdapter = (ADAPTER *)MiniportAdapterContext;

	//DbgPrint("<NdisArpFilter> -> MiniportSetInformation(%x)\n", Oid);

	do
	{
		if(Oid == OID_PNP_SET_POWER)
		{
			MiniportInternalProcessSetPowerOid(&ndisStatus,
											   pAdapter,
											   InformationBuffer,
											   InformationBufferLength,
											   BytesRead,
											   BytesNeeded);
			break;
		}

		if(pAdapter->MiniportDevicePowerState > NdisDeviceStateD0)
		{
			ndisStatus = NDIS_STATUS_FAILURE;
			break;
		}
		
		//
		// Pass Set To Lower Bound Miniport
		//
		pAdapter->Request.RequestType = NdisRequestSetInformation;
		pAdapter->Request.DATA.SET_INFORMATION.Oid = Oid;
		pAdapter->Request.DATA.SET_INFORMATION.InformationBuffer = InformationBuffer;
		pAdapter->Request.DATA.SET_INFORMATION.InformationBufferLength = InformationBufferLength;
		pAdapter->RequestBytesNeeded = BytesNeeded;
		pAdapter->RequestBytesReadOrWritten = BytesRead;

		NdisRequest(&ndisStatus, pAdapter->BindingHandle, &pAdapter->Request);
		if(ndisStatus != NDIS_STATUS_PENDING)
		{
			*BytesNeeded = pAdapter->Request.DATA.SET_INFORMATION.BytesNeeded;
			*BytesRead = pAdapter->Request.DATA.SET_INFORMATION.BytesRead;
		}
	}
	while(FALSE);

	//DbgPrint("<NdisArpFilter> <- MiniportSetInformation() [%x]\n", ndisStatus);

	return ndisStatus;
}

// ========================================================================================================================

NDIS_STATUS MiniportTransferData(OUT PNDIS_PACKET Packet,
								 OUT PUINT BytesTransferred,
								 IN NDIS_HANDLE MiniportAdapterContext,
								 IN NDIS_HANDLE MiniportReceiveContext,
								 IN UINT ByteOffset,
								 IN UINT BytesToTransfer)
{
	NDIS_STATUS ndisStatus = NDIS_STATUS_FAILURE;

	//DbgPrint("<NdisArpFilter> -> MiniportTransferData()\n");

	return ndisStatus;
}

// ========================================================================================================================

VOID MiniportInternalProcessSetPowerOid(IN OUT PNDIS_STATUS pStatus,
										IN ADAPTER *pAdapter,
										IN PVOID InformationBuffer,
										IN ULONG InformationBufferLength,
										OUT PULONG BytesRead,
										OUT PULONG BytesNeeded)
{
	NDIS_DEVICE_POWER_STATE NewDevicePowerState;

	//DbgPrint("<NdisArpFilter> -> MiniportInternalProcessSetPowerOid()\n");

	*pStatus = NDIS_STATUS_FAILURE;

	do
	{
		if(InformationBufferLength < sizeof(NDIS_DEVICE_POWER_STATE))
		{
			*pStatus = NDIS_STATUS_INVALID_LENGTH;
			break;
		}

		NewDevicePowerState = (*(PNDIS_DEVICE_POWER_STATE)InformationBuffer);
		
		if((pAdapter->MiniportDevicePowerState > NdisDeviceStateD0) && (NewDevicePowerState != NdisDeviceStateD0))
		{
			*pStatus = NDIS_STATUS_FAILURE;
			break;
		}

		pAdapter->MiniportDevicePowerState = NewDevicePowerState;
		*pStatus = NDIS_STATUS_SUCCESS;
	}
	while(FALSE);

	if(*pStatus == NDIS_STATUS_SUCCESS)
	{
		NdisMIndicateStatus(pAdapter->MiniportHandle, NDIS_STATUS_MEDIA_CONNECT, NULL, 0);
		NdisMIndicateStatusComplete(pAdapter->MiniportHandle);

		*BytesRead = sizeof(NDIS_DEVICE_POWER_STATE);
		*BytesNeeded = 0;
	}
	else
	{
		*BytesRead = 0;
		*BytesNeeded = sizeof(NDIS_DEVICE_POWER_STATE);
	}

	//DbgPrint("<NdisArpFilter> <- MiniportInternalProcessSetPowerOid() [%x]\n", *pStatus);
}

// ========================================================================================================================

VOID MiniportInternalQueryPnPCapabilities(IN OUT ADAPTER *pAdapter, OUT PNDIS_STATUS pStatus)
{
	PNDIS_PNP_CAPABILITIES pPnPCapabilities;
	PNDIS_PM_WAKE_UP_CAPABILITIES pPmWakeUpCapabilities;

	//DbgPrint("<NdisArpFilter> -> MiniportInternalQueryPnPCapabilities()\n");

	if(pAdapter->Request.DATA.QUERY_INFORMATION.InformationBufferLength >= sizeof(NDIS_PNP_CAPABILITIES))
	{
		pPnPCapabilities = (PNDIS_PNP_CAPABILITIES)(pAdapter->Request.DATA.QUERY_INFORMATION.InformationBuffer);
		pPmWakeUpCapabilities = &pPnPCapabilities->WakeUpCapabilities;
		pPmWakeUpCapabilities->MinLinkChangeWakeUp = NdisDeviceStateUnspecified;
		pPmWakeUpCapabilities->MinMagicPacketWakeUp = NdisDeviceStateUnspecified;
		pPmWakeUpCapabilities->MinPatternWakeUp = NdisDeviceStateUnspecified;

		*pAdapter->RequestBytesReadOrWritten = sizeof(NDIS_PNP_CAPABILITIES);
		*pAdapter->RequestBytesNeeded = 0;
		*pStatus = NDIS_STATUS_SUCCESS;
	}
	else
	{
		*pAdapter->RequestBytesNeeded = sizeof(NDIS_PNP_CAPABILITIES);
		*pStatus = NDIS_STATUS_RESOURCES;
	}

	//DbgPrint("<NdisArpFilter> <- MiniportInternalQueryPnPCapabilities() [%x]\n", *pStatus);
}

// ========================================================================================================================
