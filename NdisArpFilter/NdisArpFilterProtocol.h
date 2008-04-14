// ========================================================================================================================
// NdisArpFilter
//
// Copyright ©2007 Liam Kirton <liam@int3.ws>
// ========================================================================================================================
// NdisArpFilterProtocol.h
//
// Created: 02/07/2007
// ========================================================================================================================

#pragma once

// ========================================================================================================================

VOID ProtocolBindAdapter(OUT PNDIS_STATUS Status,
						 IN NDIS_HANDLE BindContext,
						 IN PNDIS_STRING DeviceName,
						 IN PVOID SystemSpecific1,
						 IN PVOID SystemSpecific2);

// ------------------------------------------------------------------------------------------------------------------------

VOID ProtocolCloseAdapterComplete(IN NDIS_HANDLE ProtocolBindingContext, IN NDIS_STATUS Status);

// ------------------------------------------------------------------------------------------------------------------------

VOID ProtocolOpenAdapterComplete(IN NDIS_HANDLE ProtocolBindingContext, IN NDIS_STATUS Status, IN NDIS_STATUS OpenErrorStatus);

// ------------------------------------------------------------------------------------------------------------------------

NDIS_STATUS ProtocolPnPEvent(IN NDIS_HANDLE ProtocolBindingContext, IN PNET_PNP_EVENT NetPnPEvent);

// ------------------------------------------------------------------------------------------------------------------------

VOID ProtocolReceiveComplete(IN NDIS_HANDLE ProtocolBindingContext);

// ------------------------------------------------------------------------------------------------------------------------

NDIS_STATUS ProtocolReceive(IN NDIS_HANDLE ProtocolBindingContext,
							IN NDIS_HANDLE MacReceiveContext,
							IN PVOID HeaderBuffer,
							IN UINT HeaderBufferSize,
							IN PVOID LookAheadBuffer,
							IN UINT LookAheadBufferSize,
							IN UINT PacketSize);

// ------------------------------------------------------------------------------------------------------------------------

INT ProtocolReceivePacket(IN NDIS_HANDLE ProtocolBindingContext, IN PNDIS_PACKET Packet);

// ------------------------------------------------------------------------------------------------------------------------

VOID ProtocolRequestComplete(IN NDIS_HANDLE ProtocolBindingContext,
							 IN PNDIS_REQUEST NdisRequest,
							 IN NDIS_STATUS Status);

// ------------------------------------------------------------------------------------------------------------------------

VOID ProtocolResetComplete(IN NDIS_HANDLE ProtocolBindingContext, IN NDIS_STATUS Status);

// ------------------------------------------------------------------------------------------------------------------------

VOID ProtocolSendComplete(IN NDIS_HANDLE ProtocolBindingContext, IN PNDIS_PACKET Packet, IN NDIS_STATUS Status);

// ------------------------------------------------------------------------------------------------------------------------

VOID ProtocolStatus(IN NDIS_HANDLE ProtocolBindingContext,
					IN NDIS_STATUS GeneralStatus,
					IN PVOID StatusBuffer,
					IN UINT StatusBufferSize);

// ------------------------------------------------------------------------------------------------------------------------

VOID ProtocolStatusComplete(IN NDIS_HANDLE ProtocolBindingContext);

// ------------------------------------------------------------------------------------------------------------------------

VOID ProtocolTransferDataComplete(IN NDIS_HANDLE ProtocolBindingContext,
								  IN PNDIS_PACKET Packet,
								  IN NDIS_STATUS Status,
								  IN UINT BytesTransferred);

// ------------------------------------------------------------------------------------------------------------------------

VOID ProtocolUnbindAdapter(OUT PNDIS_STATUS Status, IN NDIS_HANDLE ProtocolBindingContext, IN NDIS_HANDLE UnbindContext);

// ------------------------------------------------------------------------------------------------------------------------

VOID ProtocolUnload(VOID);

// ========================================================================================================================

BOOLEAN ProtocolInternalFilterPacket(IN PNDIS_PACKET Packet);

// ------------------------------------------------------------------------------------------------------------------------

VOID ProtocolInternalQueuePacket(IN ADAPTER *pAdapter, IN PNDIS_PACKET Packet, IN BOOLEAN Indicate);

// ========================================================================================================================
