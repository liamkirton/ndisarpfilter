// ========================================================================================================================
// NdisArpFilter
//
// Copyright ©2007 Liam Kirton <liam@int3.ws>
// ========================================================================================================================
// NdisArpFilter.h
//
// Created: 29/06/2007
// ========================================================================================================================

#pragma once

// ========================================================================================================================

NDIS_STATUS RegisterDevice(VOID);
NDIS_STATUS DeregisterDevice(VOID);

NTSTATUS DeviceDispatch(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp);
NTSTATUS DeviceIoControlDispatch(IN PIRP pIrp, IN PIO_STACK_LOCATION pIrpStack);

// ========================================================================================================================

extern NDIS_HANDLE g_NdisDeviceHandle;
extern NDIS_HANDLE g_NdisDriverHandle;
extern NDIS_HANDLE g_NdisProtocolHandle;
extern NDIS_HANDLE g_NdisWrapperHandle;
extern PDEVICE_OBJECT g_ControlDeviceObject;

extern NDIS_SPIN_LOCK g_Lock;
extern NDIS_SPIN_LOCK g_FilterListLock;

extern LIST_ENTRY g_AdapterList;
extern LIST_ENTRY g_FilterList;

// ========================================================================================================================

static const ULONG MemoryTag = 'ArpF';

#define MaxPacketArraySize 0x00000020
#define MinPacketPoolSize 0x000000FF
#define MaxPacketPoolSize 0x0000FFFF

static NDIS_MEDIUM SupportedMediumArray[1] =
{
	NdisMedium802_3
};

// ========================================================================================================================

typedef unsigned int u_int;
typedef unsigned short u_short;
typedef unsigned char u_char;

// ========================================================================================================================

#pragma pack(push, 1)

// ========================================================================================================================

typedef struct _EthernetFrameHeader
{
	u_char DestinationMac[6];
	u_char SourceMac[6];
	u_short Type;
} EthernetFrameHeader;

// ========================================================================================================================

typedef struct _ArpPacketHeader
{
	u_short HardwareAddressSpace;
	u_short ProtocolAddressSpace;
	u_char HardwareAddressLength;
	u_char ProtocolAddressLength;
	u_short Operation;
	u_char SenderHardwareAddress[6];
	u_int SenderProtocolAddress;
	u_char TargetHardwareAddress[6];
	u_int TargetProtocolAddress;
} ArpPacketHeader;

// ========================================================================================================================

#pragma pack(pop)

// ========================================================================================================================

typedef struct _ADAPTER
{
	LIST_ENTRY Link;
	NDIS_SPIN_LOCK AdapterLock;

	NDIS_STRING AdapterDeviceName;
	NDIS_MEDIUM AdapterMedium;
	
	NDIS_HANDLE BindingHandle;
	NDIS_HANDLE MiniportHandle;

	NDIS_DEVICE_POWER_STATE MiniportDevicePowerState;
	NDIS_DEVICE_POWER_STATE ProtocolDevicePowerState;

	NDIS_EVENT CompletionEvent;
	NDIS_STATUS CompletionStatus;

	NDIS_REQUEST Request;
	PULONG RequestBytesNeeded;
	PULONG RequestBytesReadOrWritten;

	NDIS_HANDLE RecvPacketPoolHandle;
	NDIS_HANDLE SendPacketPoolHandle;

	PNDIS_PACKET ReceivedPackets[MaxPacketArraySize];
	ULONG ReceivedPacketsCount;
} ADAPTER;

// ========================================================================================================================

typedef struct _RECV_RSVD
{
	PNDIS_PACKET Original;
} RECV_RSVD;

// ========================================================================================================================

typedef struct _SEND_RSVD
{
	PNDIS_PACKET Original;
} SEND_RSVD;

// ========================================================================================================================

typedef struct _FILTER_STRUCT
{
	LIST_ENTRY Link;
	char Mac[6];
} FILTER_STRUCT;

// ========================================================================================================================

