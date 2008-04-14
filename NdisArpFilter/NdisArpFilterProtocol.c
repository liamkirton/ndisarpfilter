// ========================================================================================================================
// NdisArpFilter
//
// Copyright ©2007 Liam Kirton <liam@int3.ws>
// ========================================================================================================================
// NdisArpFilterProtocol.c
//
// Created: 02/07/2007
// ========================================================================================================================

#include <ndis.h>

#include "NdisArpFilter.h"
#include "NdisArpFilterMiniport.h"
#include "NdisArpFilterProtocol.h"

// ========================================================================================================================

VOID ProtocolBindAdapter(OUT PNDIS_STATUS Status,
				   IN NDIS_HANDLE BindContext,
				   IN PNDIS_STRING DeviceName,
				   IN PVOID SystemSpecific1,
				   IN PVOID SystemSpecific2)
{
	NDIS_HANDLE ndisConfigurationHandle = NULL;
	PNDIS_CONFIGURATION_PARAMETER pNdisConfigurationParameter;
	NDIS_STRING ndisKeyword =  NDIS_STRING_CONST("UpperBindings");

	ADAPTER *pAdapter = NULL;
	UINT adapterSize = 0;

	NDIS_STATUS openErrorStatus = 0;
	UINT mediumIndex = 0;

	//DbgPrint("<NdisArpFilter> -> ProtocolBindAdapter()\n");

	do
	{
		//
		// Open & Read Adapter Configuration
		//
		NdisOpenProtocolConfiguration(Status, &ndisConfigurationHandle, SystemSpecific1);
		if(*Status != NDIS_STATUS_SUCCESS)
		{
			//DbgPrint("<NdisArpFilter> !! NdisOpenProtocolConfiguration() Failed [%x].\n", *Status);
			break;
		}

		NdisReadConfiguration(Status,
							  &pNdisConfigurationParameter,
							  ndisConfigurationHandle,
							  &ndisKeyword,
							  NdisParameterString);
		if(*Status != NDIS_STATUS_SUCCESS)
		{
			//DbgPrint("<NdisArpFilter> !! NdisReadConfiguration() Failed [%x].\n", *Status);
			break;
		}

		//DbgPrint("<NdisArpFilter> -- Bind Adapter: %wZ\n", &pNdisConfigurationParameter->ParameterData.StringData);

		//
		// Initialize Adapter Structure
		//
		adapterSize = sizeof(ADAPTER) + pNdisConfigurationParameter->ParameterData.StringData.MaximumLength;
		NdisAllocateMemoryWithTag(&pAdapter,
								  adapterSize,
								  MemoryTag);
		if(pAdapter == NULL)
		{
			*Status = NDIS_STATUS_RESOURCES;
			break;
		}

		NdisZeroMemory(pAdapter, adapterSize);
		pAdapter->AdapterDeviceName.Buffer = (PWCHAR)((ULONG_PTR)pAdapter + sizeof(ADAPTER));
		pAdapter->AdapterDeviceName.Length = pNdisConfigurationParameter->ParameterData.StringData.Length;
		pAdapter->AdapterDeviceName.MaximumLength = pNdisConfigurationParameter->ParameterData.StringData.MaximumLength;
		NdisMoveMemory(pAdapter->AdapterDeviceName.Buffer,
					   pNdisConfigurationParameter->ParameterData.StringData.Buffer,
					   pNdisConfigurationParameter->ParameterData.StringData.MaximumLength);

		pAdapter->ProtocolDevicePowerState = NdisDeviceStateD0;

		NdisAllocateSpinLock(&pAdapter->AdapterLock);
		NdisInitializeEvent(&pAdapter->CompletionEvent);

		//
		// Allocate Packet Pools
		//
		NdisAllocatePacketPoolEx(Status,
								 &pAdapter->RecvPacketPoolHandle,
								 MinPacketPoolSize,
								 MaxPacketPoolSize - MinPacketPoolSize,
								 PROTOCOL_RESERVED_SIZE_IN_PACKET);
		if(*Status != NDIS_STATUS_SUCCESS)
		{
			//DbgPrint("<NdisArpFilter> !! NdisAllocatePacketPoolEx() Failed [%x].\n", *Status);
			break;
		}

		NdisAllocatePacketPoolEx(Status,
								 &pAdapter->SendPacketPoolHandle,
								 MinPacketPoolSize,
								 MaxPacketPoolSize - MinPacketPoolSize,
								 sizeof(SEND_RSVD));
		if(*Status != NDIS_STATUS_SUCCESS)
		{
			//DbgPrint("<NdisArpFilter> !! NdisAllocatePacketPoolEx() Failed [%x].\n", *Status);
			break;
		}

		//
		// Open Adapter
		//
		NdisOpenAdapter(Status,
						&openErrorStatus,
						&pAdapter->BindingHandle,
						&mediumIndex,
						SupportedMediumArray,
						sizeof(SupportedMediumArray) / sizeof(NDIS_MEDIUM),
						g_NdisProtocolHandle,
						pAdapter,
						DeviceName,
						0,
						NULL);
		if(*Status == NDIS_STATUS_PENDING)
		{
			NdisWaitEvent(&pAdapter->CompletionEvent, 0);
			*Status = pAdapter->CompletionStatus;
		}
		if(*Status != NDIS_STATUS_SUCCESS)
		{
			//DbgPrint("<NdisArpFilter> !! NdisOpenAdapter() Failed [%x].\n", *Status);
			break;
		}

		pAdapter->AdapterMedium = SupportedMediumArray[mediumIndex];

		//
		// Initialize Device Instance
		//
		*Status = NdisIMInitializeDeviceInstanceEx(g_NdisDriverHandle, &pAdapter->AdapterDeviceName, pAdapter);
		if(*Status != NDIS_STATUS_SUCCESS)
		{
			//DbgPrint("<NdisArpFilter> !! NdisIMInitializeDeviceInstanceEx() Failed [%x].\n", *Status);
			break;
		}
	}
	while(FALSE);

	if(ndisConfigurationHandle != NULL)
	{
		NdisCloseConfiguration(ndisConfigurationHandle);
		ndisConfigurationHandle = NULL;
	}

	//DbgPrint("<NdisArpFilter> <- ProtocolBindAdapter() [%x]\n", *Status);
}

// ========================================================================================================================

VOID ProtocolCloseAdapterComplete(IN NDIS_HANDLE ProtocolBindingContext, IN NDIS_STATUS Status)
{
	ADAPTER *pAdapter = (ADAPTER *)ProtocolBindingContext;

	//DbgPrint("<NdisArpFilter> -> ProtocolCloseAdapterComplete() [%x]\n", Status);

	//
	// Complete Adapter Close Operation
	//
	pAdapter->CompletionStatus = Status;
	NdisSetEvent(&pAdapter->CompletionEvent);
}

// ========================================================================================================================

VOID ProtocolOpenAdapterComplete(IN NDIS_HANDLE ProtocolBindingContext, IN NDIS_STATUS Status, IN NDIS_STATUS OpenErrorStatus)
{
	ADAPTER *pAdapter = (ADAPTER *)ProtocolBindingContext;

	//DbgPrint("<NdisArpFilter> -> ProtocolOpenAdapterComplete() [%x]\n", Status);

	//
	// Complete Adapter Open Operation
	//
	pAdapter->CompletionStatus = Status;
	NdisSetEvent(&pAdapter->CompletionEvent);
}

// ========================================================================================================================

NDIS_STATUS ProtocolPnPEvent(IN NDIS_HANDLE ProtocolBindingContext, IN PNET_PNP_EVENT NetPnPEvent)
{
	//DbgPrint("<NdisArpFilter> -> ProtocolPnPEvent()\n");

	return NDIS_STATUS_SUCCESS;
}

// ========================================================================================================================

VOID ProtocolReceiveComplete(IN NDIS_HANDLE ProtocolBindingContext)
{
	ADAPTER *pAdapter = (ADAPTER *)ProtocolBindingContext;

	//DbgPrint("<NdisArpFilter> -> ProtocolReceiveComplete()\n");

	ProtocolInternalQueuePacket(pAdapter, NULL, TRUE);
	NdisMEthIndicateReceiveComplete(pAdapter->MiniportHandle);

	//DbgPrint("<NdisArpFilter> <- ProtocolReceiveComplete()\n");
}

// ========================================================================================================================

NDIS_STATUS ProtocolReceive(IN NDIS_HANDLE ProtocolBindingContext,
							IN NDIS_HANDLE MacReceiveContext,
							IN PVOID HeaderBuffer,
							IN UINT HeaderBufferSize,
							IN PVOID LookAheadBuffer,
							IN UINT LookAheadBufferSize,
							IN UINT PacketSize)
{
	ADAPTER *pAdapter = (ADAPTER *)ProtocolBindingContext;

	NDIS_STATUS ndisStatus = NDIS_STATUS_SUCCESS;

	//DbgPrint("<NdisArpFilter> -> ProtocolReceive(%d)\n", PacketSize);

	if((pAdapter->MiniportHandle == NULL) || (pAdapter->MiniportDevicePowerState > NdisDeviceStateD0))
	{
		ndisStatus = NDIS_STATUS_FAILURE;
	}
	else
	{
		do
		{
			PNDIS_PACKET pPacket = NdisGetReceivedPacket(pAdapter->BindingHandle, MacReceiveContext);
			PNDIS_PACKET pRecvPacket = NULL;

			if(pPacket != NULL)
			{
				NdisDprAllocatePacket(&ndisStatus, &pRecvPacket, pAdapter->RecvPacketPoolHandle);
				if(ndisStatus == NDIS_STATUS_SUCCESS)
				{
					NDIS_PACKET_FIRST_NDIS_BUFFER(pRecvPacket) = NDIS_PACKET_FIRST_NDIS_BUFFER(pPacket);
					NDIS_PACKET_FIRST_NDIS_BUFFER(pRecvPacket) = NDIS_PACKET_FIRST_NDIS_BUFFER(pPacket);
					NDIS_SET_ORIGINAL_PACKET(pRecvPacket, NDIS_GET_ORIGINAL_PACKET(pPacket));
					NDIS_SET_PACKET_HEADER_SIZE(pRecvPacket, HeaderBufferSize);
					
					NdisGetPacketFlags(pRecvPacket) = NdisGetPacketFlags(pPacket);
					NDIS_SET_PACKET_STATUS(pRecvPacket, NDIS_STATUS_RESOURCES);

					ProtocolInternalQueuePacket(pAdapter, pRecvPacket, TRUE);

					NdisDprFreePacket(pRecvPacket);
					break;
				}
				else
				{
					//DbgPrint("<NdisArpFilter> !! NdisDprAllocatePacket() Failed.\n");
				}
			}

			if(pRecvPacket != NULL)
			{
				ProtocolInternalQueuePacket(pAdapter, NULL, TRUE);
			}

			//DbgPrint("<NdisArpFilter> !! Fallen Through To NdisMEthIndicateReceive().\n");
			NdisMEthIndicateReceive(pAdapter->MiniportHandle,
									MacReceiveContext,
									HeaderBuffer,
									HeaderBufferSize,
									LookAheadBuffer,
									LookAheadBufferSize,
									PacketSize);
		}
		while(FALSE);
	}

	//DbgPrint("<NdisArpFilter> <- ProtocolReceive() [%x]\n", ndisStatus);

	return ndisStatus;
}

// ========================================================================================================================

INT ProtocolReceivePacket(IN NDIS_HANDLE ProtocolBindingContext, IN PNDIS_PACKET Packet)
{
	ADAPTER *pAdapter = (ADAPTER *)ProtocolBindingContext;

	BOOLEAN Remaining;
	NDIS_STATUS ndisStatus = NDIS_STATUS_SUCCESS;

	//DbgPrint("<NdisArpFilter> -> ProtocolReceivePacket()\n");

	if((pAdapter->MiniportHandle == NULL) || (pAdapter->MiniportDevicePowerState > NdisDeviceStateD0))
	{
		return 0;
	}

	NdisIMGetCurrentPacketStack(Packet, &Remaining);
	if(Remaining)
	{
		ndisStatus = NDIS_GET_PACKET_STATUS(Packet);
		ProtocolInternalQueuePacket(pAdapter, Packet, (ndisStatus == NDIS_STATUS_RESOURCES));
	}
	else
	{
		PNDIS_PACKET pRecvPacket;

		NdisDprAllocatePacket(&ndisStatus, &pRecvPacket, pAdapter->RecvPacketPoolHandle);
		if(ndisStatus == NDIS_STATUS_SUCCESS)
		{
			RECV_RSVD *pRecvRsvd = (RECV_RSVD *)pRecvPacket->MiniportReserved;
			pRecvRsvd->Original = Packet;

			NDIS_PACKET_FIRST_NDIS_BUFFER(pRecvPacket) = NDIS_PACKET_FIRST_NDIS_BUFFER(Packet);
			NDIS_PACKET_FIRST_NDIS_BUFFER(pRecvPacket) = NDIS_PACKET_FIRST_NDIS_BUFFER(Packet);
			NDIS_SET_ORIGINAL_PACKET(pRecvPacket, NDIS_GET_ORIGINAL_PACKET(Packet));
			
			NdisGetPacketFlags(pRecvPacket) = NdisGetPacketFlags(Packet);

			ndisStatus = NDIS_GET_PACKET_STATUS(Packet);
			NDIS_SET_PACKET_STATUS(pRecvPacket, ndisStatus);

			ProtocolInternalQueuePacket(pAdapter, pRecvPacket, (ndisStatus == NDIS_STATUS_RESOURCES));

			if(ndisStatus == NDIS_STATUS_RESOURCES)
			{
				NdisDprFreePacket(pRecvPacket);
			}
		}
		else
		{
			//DbgPrint("<NdisArpFilter> !! NdisDprAllocatePacket() Failed.\n");
		}
	}

	//DbgPrint("<NdisArpFilter> <- ProtocolReceivePacket()\n");
	
	return (ndisStatus != NDIS_STATUS_RESOURCES) ? 1 : 0;
}

// ========================================================================================================================

VOID ProtocolRequestComplete(IN NDIS_HANDLE ProtocolBindingContext,
							 IN PNDIS_REQUEST NdisRequest,
							 IN NDIS_STATUS Status)
{
	ADAPTER *pAdapter = (ADAPTER *)ProtocolBindingContext;
	NDIS_OID Oid = pAdapter->Request.DATA.SET_INFORMATION.Oid;

	//DbgPrint("<NdisArpFilter> -> ProtocolRequestComplete()\n");

	switch(NdisRequest->RequestType)
	{
		case NdisRequestQueryInformation:
			if((Oid == OID_PNP_CAPABILITIES) && (Status == NDIS_STATUS_SUCCESS))
			{
				MiniportInternalQueryPnPCapabilities(pAdapter, &Status);
			}
			
			*pAdapter->RequestBytesNeeded = NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded;
			*pAdapter->RequestBytesReadOrWritten = NdisRequest->DATA.QUERY_INFORMATION.BytesWritten;
			
			if((Oid == OID_GEN_MAC_OPTIONS) && (Status == NDIS_STATUS_SUCCESS))
			{
				*((PULONG)NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer) &= ~NDIS_MAC_OPTION_NO_LOOPBACK;
			}

			NdisMQueryInformationComplete(pAdapter->MiniportHandle, Status);
			break;

		case NdisRequestSetInformation:
			*pAdapter->RequestBytesNeeded = NdisRequest->DATA.SET_INFORMATION.BytesNeeded;
			*pAdapter->RequestBytesReadOrWritten = NdisRequest->DATA.SET_INFORMATION.BytesRead;

			NdisMSetInformationComplete(pAdapter->MiniportHandle, Status);
			break;
	}

	//DbgPrint("<NdisArpFilter> <- ProtocolRequestComplete()\n");
}

// ========================================================================================================================

VOID ProtocolResetComplete(IN NDIS_HANDLE ProtocolBindingContext, IN NDIS_STATUS Status)
{
	//DbgPrint("<NdisArpFilter> -> ProtocolResetComplete()\n");
}

// ========================================================================================================================

VOID ProtocolSendComplete(IN NDIS_HANDLE ProtocolBindingContext, IN PNDIS_PACKET Packet, IN NDIS_STATUS Status)
{
	ADAPTER *pAdapter = (ADAPTER *)ProtocolBindingContext;

	NDIS_HANDLE PoolHandle;
	
	//DbgPrint("<NdisArpFilter> -> ProtocolSendComplete()\n");

	PoolHandle = NdisGetPoolFromPacket(Packet);
	if(PoolHandle != pAdapter->SendPacketPoolHandle)
	{
		NdisMSendComplete(pAdapter->MiniportHandle, Packet, Status);
	}
	else
	{
		SEND_RSVD *pSendRsvd = (SEND_RSVD *)Packet->ProtocolReserved;
		PNDIS_PACKET pSentPacket = pSendRsvd->Original;

		NdisIMCopySendCompletePerPacketInfo(pSentPacket, Packet);
		NdisDprFreePacket(Packet);
		NdisMSendComplete(pAdapter->MiniportHandle, pSentPacket, Status);
	}
}

// ========================================================================================================================

VOID ProtocolStatus(IN NDIS_HANDLE ProtocolBindingContext,
			  IN NDIS_STATUS GeneralStatus,
			  IN PVOID StatusBuffer,
			  IN UINT StatusBufferSize)
{
	//DbgPrint("<NdisArpFilter> -> ProtocolStatus()\n");
}

// ========================================================================================================================

VOID ProtocolStatusComplete(IN NDIS_HANDLE ProtocolBindingContext)
{
	//DbgPrint("<NdisArpFilter> -> ProtocolStatusComplete()\n");
}

// ========================================================================================================================

VOID ProtocolTransferDataComplete(IN NDIS_HANDLE ProtocolBindingContext,
							IN PNDIS_PACKET Packet,
							IN NDIS_STATUS Status,
							IN UINT BytesTransferred)
{
	//DbgPrint("<NdisArpFilter> -> ProtocolTransferDataComplete()\n");
}

// ========================================================================================================================

VOID ProtocolUnbindAdapter(OUT PNDIS_STATUS Status, IN NDIS_HANDLE ProtocolBindingContext, IN NDIS_HANDLE UnbindContext)
{
	ADAPTER *pAdapter = (ADAPTER *)ProtocolBindingContext;

	NDIS_PACKET PacketArray[MaxPacketArraySize];
	ULONG NumberOfPackets = 0;

	//DbgPrint("<NdisArpFilter> -> ProtocolUnbindAdapter(%x)\n", pAdapter);

	NdisAcquireSpinLock(&pAdapter->AdapterLock);
	if(pAdapter->ReceivedPacketsCount > 0)
	{
		NdisMoveMemory(PacketArray, pAdapter->ReceivedPackets, pAdapter->ReceivedPacketsCount * sizeof(PNDIS_PACKET));
		NumberOfPackets = pAdapter->ReceivedPacketsCount;
		pAdapter->ReceivedPacketsCount = 0;
	}
	NdisReleaseSpinLock(&pAdapter->AdapterLock);

	if(NumberOfPackets > 0)
	{
		ULONG i = 0;
		for(i = 0; i < NumberOfPackets; ++i)
		{
			MiniportReturnPacket(pAdapter, &PacketArray[i]);
		}
	}

	if(pAdapter->MiniportHandle != NULL)
	{
		*Status = NdisIMDeInitializeDeviceInstance(pAdapter->MiniportHandle);
		if(*Status != NDIS_STATUS_SUCCESS)
		{
			*Status = NDIS_STATUS_FAILURE;
		}
	}
	else
	{
		//
		// Close Adapter Handle
		//
		if(pAdapter->BindingHandle != NULL)
		{
			NdisResetEvent(&pAdapter->CompletionEvent);

			NdisCloseAdapter(Status, pAdapter->BindingHandle);
			if(*Status == NDIS_STATUS_PENDING)
			{
				NdisWaitEvent(&pAdapter->CompletionEvent, 0);
				*Status = pAdapter->CompletionStatus;
			}

			if(*Status != NDIS_STATUS_SUCCESS)
			{
				//DbgPrint("<NdisArpFilter> !! NdisCloseAdapter() Failed [%x].\n", *Status);
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
	}
}

// ========================================================================================================================

VOID ProtocolUnload(VOID)
{
	//DbgPrint("<NdisArpFilter> -> ProtocolUnload()\n");

	if(g_NdisProtocolHandle != NULL)
	{
		NDIS_STATUS ndisStatus;
		NdisDeregisterProtocol(&ndisStatus, g_NdisProtocolHandle);
		g_NdisProtocolHandle = NULL;
	}
}

// ========================================================================================================================

BOOLEAN ProtocolInternalFilterPacket(IN PNDIS_PACKET Packet)
{
	UINT PhysicalBufferCount;
    UINT BufferCount;
    PNDIS_BUFFER FirstBuffer;
    UINT TotalPacketLength;
	PVOID VirtualAddress;
	ULONG Length;

	BOOLEAN bReturn = FALSE;

	//DbgPrint("<NdisArpFilter> -> ProtocolInternalFilterPacket(%x)\n", Packet);

	NdisQueryPacket(Packet, &PhysicalBufferCount, &BufferCount, &FirstBuffer, &TotalPacketLength);	
	NdisQueryBuffer(FirstBuffer, &VirtualAddress, &Length);

	if(Length >= sizeof(EthernetFrameHeader) + sizeof(ArpPacketHeader))
	{
		EthernetFrameHeader *pEthernetFrameHeader = (EthernetFrameHeader *)VirtualAddress;
		ArpPacketHeader *pArpPacketHeader = (ArpPacketHeader *)(((UCHAR *)VirtualAddress) + sizeof(EthernetFrameHeader));

		if(pEthernetFrameHeader->Type == 0x0608)
		{
			if(pArpPacketHeader->Operation == 0x0200)
			{
				FILTER_STRUCT *pFilter = NULL;
				PLIST_ENTRY pFilterIter = NULL;

				NdisAcquireSpinLock(&g_FilterListLock);
				for(pFilterIter = g_FilterList.Flink; pFilterIter != &g_FilterList; pFilterIter = pFilterIter->Flink)
				{
					pFilter = CONTAINING_RECORD(pFilterIter, FILTER_STRUCT, Link);
					if(RtlEqualMemory(&pFilter->Mac, &pEthernetFrameHeader->SourceMac, 6))
					{
						DbgPrint("<NdisArpFilter> ** DROP ARP %03d.%03d.%03d.%03d IS-AT %02x::%02x::%02x::%02x::%02x::%02x",
								 (pArpPacketHeader->SenderProtocolAddress & 0x000000FF),
								 ((pArpPacketHeader->SenderProtocolAddress & 0x0000FF00) >> 8),
								 ((pArpPacketHeader->SenderProtocolAddress & 0x00FF0000) >> 16),
								 ((pArpPacketHeader->SenderProtocolAddress & 0xFF000000) >> 24),
								 pEthernetFrameHeader->SourceMac[0],
								 pEthernetFrameHeader->SourceMac[1],
								 pEthernetFrameHeader->SourceMac[2],
								 pEthernetFrameHeader->SourceMac[3],
								 pEthernetFrameHeader->SourceMac[4],
								 pEthernetFrameHeader->SourceMac[5]);
						
						bReturn = TRUE;
						break;
					}
					pFilter = NULL;
				}
				NdisReleaseSpinLock(&g_FilterListLock);
			}
		}
	}
	
	return bReturn;
}

// ========================================================================================================================

VOID ProtocolInternalQueuePacket(IN ADAPTER *pAdapter, IN PNDIS_PACKET Packet, IN BOOLEAN Indicate)
{
	PNDIS_PACKET PacketArray[MaxPacketArraySize];
	ULONG NumberOfPackets = 0;

	//DbgPrint("<NdisArpFilter> -> ProtocolInternalQueuePacket()\n");

	NdisDprAcquireSpinLock(&pAdapter->AdapterLock);
	if(Packet != NULL)
	{
		pAdapter->ReceivedPackets[pAdapter->ReceivedPacketsCount++] = Packet;
	}
	if((pAdapter->ReceivedPacketsCount == MaxPacketArraySize) ||
	   Indicate ||
	   ((Packet == NULL) && (pAdapter->ReceivedPacketsCount > 0)))
	{
		NdisMoveMemory(PacketArray, pAdapter->ReceivedPackets, pAdapter->ReceivedPacketsCount * sizeof(PNDIS_PACKET));
		NumberOfPackets = pAdapter->ReceivedPacketsCount;
		pAdapter->ReceivedPacketsCount = 0;
	}
	NdisDprReleaseSpinLock(&pAdapter->AdapterLock);

	if(NumberOfPackets != 0)
	{
		UINT i = 0;
		for (i = 0; i < NumberOfPackets; ++i)
		{
			if((pAdapter->MiniportHandle != NULL) &&
			   (pAdapter->MiniportDevicePowerState == NdisDeviceStateD0) &&
			   !ProtocolInternalFilterPacket(PacketArray[i]))
			{
				//DbgPrint("<NdisArpFilter> -- NdisMIndicateReceivePacket(%d) @ %d\n", NumberOfPackets, KeGetCurrentIrql());
				NdisMIndicateReceivePacket(pAdapter->MiniportHandle, &PacketArray[i], 1);
			}
			else
			{
				MiniportReturnPacket(pAdapter, PacketArray[i]);
			}
		}
	}

	//DbgPrint("<NdisArpFilter> <- ProtocolInternalQueuePacket()\n");
}

// ========================================================================================================================
