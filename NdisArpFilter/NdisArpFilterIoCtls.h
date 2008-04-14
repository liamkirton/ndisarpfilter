// ========================================================================================================================
// NdisArpFilter
//
// Copyright ©2007 Liam Kirton <liam@int3.ws>
// ========================================================================================================================
// NdisArpFilterIoCtls.h
//
// Created: 08/07/2007
// ========================================================================================================================

#pragma once

// ========================================================================================================================

#define IOCTL_ADD_ARP_FILTER CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_REM_ARP_FILTER CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_LST_ARP_FILTER CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

// ========================================================================================================================
