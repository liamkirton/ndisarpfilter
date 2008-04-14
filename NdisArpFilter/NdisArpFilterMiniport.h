// ========================================================================================================================
// NdisArpFilter
//
// Copyright ©2007 Liam Kirton <liam@int3.ws>
// ========================================================================================================================
// NdisArpFilterMiniport.h
//
// Created: 02/07/2007
// ========================================================================================================================

#pragma once

// ========================================================================================================================

VOID MiniportAdapterShutdown(IN PVOID ShutdownContext);

// ------------------------------------------------------------------------------------------------------------------------

VOID MiniportCancelSendPackets(IN NDIS_HANDLE MiniportAdapterContext, IN PVOID CancelId);

// ------------------------------------------------------------------------------------------------------------------------

NDIS_STATUS MiniportInitialize(OUT PNDIS_STATUS OpenErrorStatus,
							   OUT PUINT SelectedMediumIndex,
							   IN PNDIS_MEDIUM MediumArray,
							   IN UINT MediumArraySize,
							   IN NDIS_HANDLE MiniportAdapterHandle,
							   IN NDIS_HANDLE WrapperConfigurationContext);

// ------------------------------------------------------------------------------------------------------------------------

VOID MiniportHalt(IN NDIS_HANDLE MiniportAdapterContext);

// ------------------------------------------------------------------------------------------------------------------------

VOID MiniportPnPEventNotify(IN NDIS_HANDLE MiniportAdapterContext,
							IN NDIS_DEVICE_PNP_EVENT PnPEvent,
							IN PVOID InformationBuffer,
							IN ULONG InformationBufferLength);

// ------------------------------------------------------------------------------------------------------------------------

NDIS_STATUS MiniportQueryInformation(IN NDIS_HANDLE MiniportAdapterContext,
									 IN NDIS_OID Oid,
									 IN PVOID InformationBuffer,
									 IN ULONG InformationBufferLength,
									 OUT PULONG BytesWritten,
									 OUT PULONG BytesNeeded);

// ------------------------------------------------------------------------------------------------------------------------

VOID MiniportReturnPacket(IN NDIS_HANDLE MiniportAdapterContext, IN PNDIS_PACKET Packet);

// ------------------------------------------------------------------------------------------------------------------------

VOID MiniportSendPackets(IN NDIS_HANDLE MiniportAdapterContext, IN PPNDIS_PACKET PacketArray, IN UINT NumberOfPackets);

// ------------------------------------------------------------------------------------------------------------------------

NDIS_STATUS MiniportSetInformation(IN NDIS_HANDLE MiniportAdapterContext,
								   IN NDIS_OID  Oid,
								   IN PVOID  InformationBuffer,
								   IN ULONG  InformationBufferLength,
								   OUT PULONG  BytesRead,
								   OUT PULONG  BytesNeeded);

// ------------------------------------------------------------------------------------------------------------------------

NDIS_STATUS MiniportTransferData(OUT PNDIS_PACKET Packet,
								 OUT PUINT BytesTransferred,
								 IN NDIS_HANDLE MiniportAdapterContext,
								 IN NDIS_HANDLE MiniportReceiveContext,
								 IN UINT ByteOffset,
								 IN UINT BytesToTransfer);

// ========================================================================================================================

VOID MiniportInternalProcessSetPowerOid(IN OUT PNDIS_STATUS pStatus,
										IN ADAPTER *pAdapter,
										IN PVOID InformationBuffer,
										IN ULONG InformationBufferLength,
										OUT PULONG BytesRead,
										OUT PULONG BytesNeeded);

// ------------------------------------------------------------------------------------------------------------------------

VOID MiniportInternalQueryPnPCapabilities(IN OUT ADAPTER *pAdapter, OUT PNDIS_STATUS pStatus);

// ========================================================================================================================
