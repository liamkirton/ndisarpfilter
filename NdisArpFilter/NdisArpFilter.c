// ========================================================================================================================
// NdisArpFilter
//
// Copyright ©2007 Liam Kirton <liam@int3.ws>
// ========================================================================================================================
// NdisArpFilter.c
//
// Created: 29/06/2007
// ========================================================================================================================

#include <ndis.h>

#include "NdisArpFilter.h"
#include "NdisArpFilterIoCtls.h"
#include "NdisArpFilterMiniport.h"
#include "NdisArpFilterProtocol.h"

// ========================================================================================================================

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegistryPath);
VOID DriverUnload(IN PDRIVER_OBJECT pDriverObject);

#pragma NDIS_INIT_FUNCTION(DriverEntry)

// ========================================================================================================================

NDIS_HANDLE g_NdisDeviceHandle;
NDIS_HANDLE g_NdisDriverHandle;
NDIS_HANDLE g_NdisProtocolHandle;
NDIS_HANDLE g_NdisWrapperHandle;
PDEVICE_OBJECT g_ControlDeviceObject;

NDIS_SPIN_LOCK g_Lock;
NDIS_SPIN_LOCK g_FilterListLock;

LIST_ENTRY g_AdapterList;
LIST_ENTRY g_FilterList;

ULONG g_MiniportCount = 0;

// ========================================================================================================================

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegistryPath)
{
	NDIS_STATUS ndisStatus = NDIS_STATUS_SUCCESS;

	NDIS_MINIPORT_CHARACTERISTICS ndisMiniportCharacteristics;
	NDIS_PROTOCOL_CHARACTERISTICS ndisProtocolCharacteristics;
	NDIS_STRING ndisProtocolName;

	DbgPrint("NdisArpFilter 0.1.1\n");
	DbgPrint("\n");
	DbgPrint("Copyright (C)2007 Liam Kirton <liam@int3.ws>\n");
	DbgPrint("\n");
	DbgPrint("Built at %s on %s\n", __TIME__, __DATE__);
	DbgPrint("\n");
	
	//DbgPrint("<NdisArpFilter> -> DriverEntry()\n");

	//
	// Initialize Globals
	//
	InitializeListHead(&g_AdapterList);
	InitializeListHead(&g_FilterList);
	
	NdisAllocateSpinLock(&g_Lock);
	NdisAllocateSpinLock(&g_FilterListLock);
	
	NdisMInitializeWrapper(&g_NdisWrapperHandle, pDriverObject, pRegistryPath, NULL);
	NdisMRegisterUnloadHandler(g_NdisWrapperHandle, DriverUnload);

	do
	{
		//
		// Register Miniport With NDIS
		//
		NdisZeroMemory(&ndisMiniportCharacteristics, sizeof(NDIS_MINIPORT_CHARACTERISTICS));
		ndisMiniportCharacteristics.MajorNdisVersion = 5;
		ndisMiniportCharacteristics.MinorNdisVersion = 1;
		
		ndisMiniportCharacteristics.AdapterShutdownHandler = MiniportAdapterShutdown;
		ndisMiniportCharacteristics.CancelSendPacketsHandler = MiniportCancelSendPackets;
		ndisMiniportCharacteristics.CheckForHangHandler = NULL;
		ndisMiniportCharacteristics.HaltHandler = MiniportHalt;
		ndisMiniportCharacteristics.InitializeHandler = MiniportInitialize;
		ndisMiniportCharacteristics.PnPEventNotifyHandler = MiniportPnPEventNotify;
		ndisMiniportCharacteristics.QueryInformationHandler = MiniportQueryInformation;
		ndisMiniportCharacteristics.ResetHandler = NULL;
		ndisMiniportCharacteristics.ReturnPacketHandler = MiniportReturnPacket;
		ndisMiniportCharacteristics.SendHandler = NULL;
		ndisMiniportCharacteristics.SendPacketsHandler = MiniportSendPackets;
		ndisMiniportCharacteristics.SetInformationHandler = MiniportSetInformation;
		ndisMiniportCharacteristics.TransferDataHandler = MiniportTransferData;

		ndisStatus = NdisIMRegisterLayeredMiniport(g_NdisWrapperHandle,
												   &ndisMiniportCharacteristics,
												   sizeof(NDIS_MINIPORT_CHARACTERISTICS),
												   &g_NdisDriverHandle);
		if(ndisStatus != NDIS_STATUS_SUCCESS)
		{
			//DbgPrint("<NdisArpFilter> !! NdisIMRegisterLayeredMiniport() Failed [%x].\n", ndisStatus);
			break;
		}

		//
		// Register Protocol With NDIS
		//
		NdisZeroMemory(&ndisProtocolCharacteristics, sizeof(NDIS_PROTOCOL_CHARACTERISTICS));
		ndisProtocolCharacteristics.MajorNdisVersion = 5;
		ndisProtocolCharacteristics.MinorNdisVersion = 1;

		NdisInitUnicodeString(&ndisProtocolName, L"NdisArpFilter");
		ndisProtocolCharacteristics.Name = ndisProtocolName;

		ndisProtocolCharacteristics.BindAdapterHandler = ProtocolBindAdapter;
		ndisProtocolCharacteristics.CloseAdapterCompleteHandler = ProtocolCloseAdapterComplete;
		ndisProtocolCharacteristics.OpenAdapterCompleteHandler = ProtocolOpenAdapterComplete;
		ndisProtocolCharacteristics.PnPEventHandler = ProtocolPnPEvent;
		ndisProtocolCharacteristics.ReceiveHandler = ProtocolReceive;
		ndisProtocolCharacteristics.ReceiveCompleteHandler = ProtocolReceiveComplete;
		ndisProtocolCharacteristics.ReceivePacketHandler = ProtocolReceivePacket;
		ndisProtocolCharacteristics.RequestCompleteHandler = ProtocolRequestComplete;
		ndisProtocolCharacteristics.ResetCompleteHandler = ProtocolResetComplete;
		ndisProtocolCharacteristics.SendCompleteHandler = ProtocolSendComplete;
		ndisProtocolCharacteristics.StatusHandler = ProtocolStatus;
		ndisProtocolCharacteristics.StatusCompleteHandler = ProtocolStatusComplete;
		ndisProtocolCharacteristics.TransferDataCompleteHandler = ProtocolTransferDataComplete;
		ndisProtocolCharacteristics.UnbindAdapterHandler = ProtocolUnbindAdapter;
		ndisProtocolCharacteristics.UnloadHandler = ProtocolUnload;

		NdisRegisterProtocol(&ndisStatus,
							 &g_NdisProtocolHandle,
							 &ndisProtocolCharacteristics,
							 sizeof(NDIS_PROTOCOL_CHARACTERISTICS));
		if(ndisStatus != NDIS_STATUS_SUCCESS)
		{
			//DbgPrint("<NdisArpFilter> !! NdisRegisterProtocol() Failed [%x].\n", ndisStatus);

			NdisIMDeregisterLayeredMiniport(&g_NdisDriverHandle);
			break;
		}

		NdisIMAssociateMiniport(g_NdisDriverHandle, &g_NdisProtocolHandle);
	}
	while(FALSE);

	if(ndisStatus != NDIS_STATUS_SUCCESS)
	{
		NdisTerminateWrapper(g_NdisWrapperHandle, NULL);
	}

	//DbgPrint("<NdisArpFilter> <- DriverEntry() [%x]\n", ndisStatus);

	return ndisStatus;
}

// ========================================================================================================================

VOID DriverUnload(IN PDRIVER_OBJECT pDriverObject)
{
	//DbgPrint("<NdisArpFilter> -> DriverUnload()\n");

	//
	// Deregister Protocol
	//
	ProtocolUnload();

	//
	// Deregister Miniport
	//
	NdisIMDeregisterLayeredMiniport(g_NdisDriverHandle);

	//
	// Destroy Globals
	//
	NdisFreeSpinLock(&g_Lock);

	//DbgPrint("<NdisArpFilter> <- DriverUnload()\n");
}

// ========================================================================================================================

NDIS_STATUS RegisterDevice(VOID)
{
	NDIS_STATUS ndisStatus = NDIS_STATUS_SUCCESS;
	PDRIVER_DISPATCH dispatchTable[IRP_MJ_MAXIMUM_FUNCTION + 1];
	UNICODE_STRING deviceName;
	UNICODE_STRING deviceLink;	

	//DbgPrint("<NdisArpFilter> -> RegisterDevice()\n");
	NdisAcquireSpinLock(&g_Lock);

	if(++g_MiniportCount == 1)
	{
		//
		// Register Device Object
		//
		NdisZeroMemory(&dispatchTable, sizeof(dispatchTable));
		dispatchTable[IRP_MJ_CREATE] = DeviceDispatch;
		dispatchTable[IRP_MJ_CLEANUP] = DeviceDispatch;
		dispatchTable[IRP_MJ_CLOSE] = DeviceDispatch;
		dispatchTable[IRP_MJ_DEVICE_CONTROL] = DeviceDispatch;

		NdisInitUnicodeString(&deviceName, L"\\DosDevices\\NdisArpFilter");
		NdisInitUnicodeString(&deviceLink, L"\\Device\\NdisArpFilter");

		ndisStatus = NdisMRegisterDevice(g_NdisWrapperHandle,
										 &deviceName,
										 &deviceLink,
										 &dispatchTable[0],
										 &g_ControlDeviceObject,
										 &g_NdisDeviceHandle);
		if(ndisStatus != NDIS_STATUS_SUCCESS)
		{
			//DbgPrint("<NdisArpFilter> !! NdisMRegisterDevice() Failed [%x].\n", ndisStatus);
		}
	}

	NdisReleaseSpinLock(&g_Lock);
	//DbgPrint("<NdisArpFilter> <- RegisterDevice() [%x]\n", ndisStatus);

	return ndisStatus;
}

// ========================================================================================================================

NDIS_STATUS DeregisterDevice(VOID)
{
	NDIS_STATUS ndisStatus = NDIS_STATUS_SUCCESS;

	//DbgPrint("<NdisArpFilter> -> DeregisterDevice()\n");
	NdisAcquireSpinLock(&g_Lock);

	if(--g_MiniportCount == 0)
	{
		//
		// Deregister Device Object
		//
		if(g_NdisDeviceHandle != NULL)
		{
			ndisStatus = NdisMDeregisterDevice(g_NdisDeviceHandle);
			if(ndisStatus != NDIS_STATUS_SUCCESS)
			{
				//DbgPrint("<NdisArpFilter> !! NdisMDeregisterDevice() Failed [%x].\n", ndisStatus);
			}
			g_NdisDeviceHandle = NULL;
		}
	}

	NdisReleaseSpinLock(&g_Lock);
	//DbgPrint("<NdisArpFilter> <- DeregisterDevice() [%x]\n", ndisStatus);

	return ndisStatus;
}

// ========================================================================================================================

NTSTATUS DeviceDispatch(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PIO_STACK_LOCATION pIrpStack;

	//DbgPrint("<NdisArpFilter> -> DeviceDispatch()\n");

	//
	// Handle IRP
	//
	pIrpStack = IoGetCurrentIrpStackLocation(pIrp);

	switch(pIrpStack->MajorFunction)
	{
		case IRP_MJ_CREATE:
			break;

		case IRP_MJ_CLEANUP:
			break;

		case IRP_MJ_CLOSE:
			break;

		case IRP_MJ_DEVICE_CONTROL:
			ntStatus = DeviceIoControlDispatch(pIrp, pIrpStack);
			break;
	}

	pIrp->IoStatus.Status = ntStatus;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	//DbgPrint("<NdisArpFilter> <- DeviceDispatch() [%x]\n", ntStatus);

	return ntStatus;
}

// ========================================================================================================================

NTSTATUS DeviceIoControlDispatch(IN PIRP pIrp, IN PIO_STACK_LOCATION pIrpStack)
{
	char *pMacBuffer = (char *)pIrp->AssociatedIrp.SystemBuffer;

	UINT i = 0;

	FILTER_STRUCT *pFilter = NULL;
	PLIST_ENTRY pFilterIter = NULL;

	NTSTATUS ntStatus = STATUS_SUCCESS;

	//DbgPrint("<NdisArpFilter> -> DeviceIoControlDispatch()\n");

	if(pMacBuffer == NULL)
	{
		//DbgPrint("<NdisArpFilter> !! pMacBuffer == NULL\n");
		return STATUS_INVALID_BUFFER_SIZE;
	}

	switch(pIrpStack->Parameters.DeviceIoControl.IoControlCode)
	{
		case IOCTL_ADD_ARP_FILTER:
			//DbgPrint("<NdisArpFilter> -- IOCTL_ADD_ARP_FILTER\n");
			if(pIrpStack->Parameters.DeviceIoControl.InputBufferLength != 6)
			{
				//DbgPrint("<NdisArpFilter> !! DeviceIoControl.InputBufferLength != 6\n");
			}
			else
			{
				DbgPrint("<NdisArpFilter> -- ADD ARP FILTER %02x::%02x::%02x::%02x::%02x::%02x",
						 pMacBuffer[0],
						 pMacBuffer[1],
						 pMacBuffer[2],
						 pMacBuffer[3],
						 pMacBuffer[4],
						 pMacBuffer[5]);

				NdisAcquireSpinLock(&g_FilterListLock);
				for(pFilterIter = g_FilterList.Flink; pFilterIter != &g_FilterList; pFilterIter = pFilterIter->Flink)
				{
					pFilter = CONTAINING_RECORD(pFilterIter, FILTER_STRUCT, Link);
					if(RtlEqualMemory(&pFilter->Mac, pMacBuffer, 6))
					{
						break;
					}
					pFilter = NULL;
				}
				NdisReleaseSpinLock(&g_FilterListLock);

				if(pFilter == NULL)
				{
					NdisAllocateMemoryWithTag(&pFilter, sizeof(FILTER_STRUCT), MemoryTag);
					RtlCopyMemory(&pFilter->Mac, pMacBuffer, 6);

					NdisAcquireSpinLock(&g_FilterListLock);
					InsertTailList(&g_FilterList, &pFilter->Link);
					NdisReleaseSpinLock(&g_FilterListLock);
				}
				else
				{
					ntStatus = STATUS_INVALID_BUFFER_SIZE;
				}
			}
			break;

		case IOCTL_REM_ARP_FILTER:
			//DbgPrint("<NdisArpFilter> -- IOCTL_REM_ARP_FILTER\n");
			if(pIrpStack->Parameters.DeviceIoControl.InputBufferLength != 6)
			{
				//DbgPrint("<NdisArpFilter> !! DeviceIoControl.InputBufferLength != 6\n");
			}
			else
			{
				DbgPrint("<NdisArpFilter> -- REM ARP FILTER %02x::%02x::%02x::%02x::%02x::%02x\n",
						 pMacBuffer[0],
						 pMacBuffer[1],
						 pMacBuffer[2],
						 pMacBuffer[3],
						 pMacBuffer[4],
						 pMacBuffer[5]);

				NdisAcquireSpinLock(&g_FilterListLock);
				for(pFilterIter = g_FilterList.Flink; pFilterIter != &g_FilterList; pFilterIter = pFilterIter->Flink)
				{
					pFilter = CONTAINING_RECORD(pFilterIter, FILTER_STRUCT, Link);
					if(RtlEqualMemory(&pFilter->Mac, pMacBuffer, 6))
					{
						break;
					}
					pFilter = NULL;
				}
				NdisReleaseSpinLock(&g_FilterListLock);

				if(pFilter != NULL)
				{
					NdisAcquireSpinLock(&g_FilterListLock);
					RemoveEntryList(&pFilter->Link);
					NdisReleaseSpinLock(&g_FilterListLock);

					NdisFreeMemory(pFilter, sizeof(FILTER_STRUCT), 0);
				}
				else
				{
					ntStatus = STATUS_INVALID_BUFFER_SIZE;
				}
			}
			break;

		case IOCTL_LST_ARP_FILTER:
			//DbgPrint("<NdisArpFilter> -- IOCTL_LST_ARP_FILTER\n");
			if((pIrpStack->Parameters.DeviceIoControl.OutputBufferLength == 0) ||
			   ((pIrpStack->Parameters.DeviceIoControl.OutputBufferLength % 6) != 0))
			{
				//DbgPrint("<NdisArpFilter> !! (DeviceIoControl.OutputBufferLength == 0) || ((DeviceIoControl.OutputBufferLength % 6) != 0)\n");
			}
			else
			{
				i = 0;
				NdisAcquireSpinLock(&g_FilterListLock);
				for(pFilterIter = g_FilterList.Flink; pFilterIter != &g_FilterList; pFilterIter = pFilterIter->Flink)
				{
					pFilter = CONTAINING_RECORD(pFilterIter, FILTER_STRUCT, Link);
					if((i + 6) > pIrpStack->Parameters.DeviceIoControl.OutputBufferLength)
					{
						break;
					}

					RtlCopyMemory(&pMacBuffer[i], &pFilter->Mac, 6);
					i += 6;
				}
				NdisReleaseSpinLock(&g_FilterListLock);
				pIrp->IoStatus.Information = i;
			}
			break;
	}

	return ntStatus;
}

// ========================================================================================================================
