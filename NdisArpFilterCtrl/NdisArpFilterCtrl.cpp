// ========================================================================================================================
// NdisArpFilterCtrl
//
// Copyright ©2007 Liam Kirton <liam@int3.ws>
// ========================================================================================================================
// NdisArpFilterCtrl.cpp
//
// Created: 29/06/2007
// ========================================================================================================================

#include <windows.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>

#include "../NdisArpFilter/NdisArpFilterIoCtls.h"

// ========================================================================================================================

void PrintUsage();

// ========================================================================================================================

const char *g_cNdisArpFilterCtrlVersion = "0.1.1";

// ========================================================================================================================

int main(int argc, char *argv[])
{
	std::cout << std::endl
			  << "NdisArpFilterCtrl " << g_cNdisArpFilterCtrlVersion << std::endl
			  << "Copyright " << "\xB8" << "2007 Liam Kirton <liam@int3.ws>" << std::endl
			  << std::endl
			  << "Built at " << __TIME__ << " on " << __DATE__ << std::endl << std::endl;

	HANDLE hDevice = NULL;
	
	try
	{
		if((hDevice = CreateFile(L"\\\\.\\NdisArpFilter",
								 GENERIC_READ | GENERIC_WRITE,
								 0,
								 NULL,
								 OPEN_EXISTING,
								 0,
								 NULL)) == INVALID_HANDLE_VALUE)
		{
			throw std::exception("CreateFile(\"\\\\.\\NdisArpFilter\") Failed.");
		}

		for(int i = 1; i < argc; ++i)
		{
			std::string cmd = argv[i];
			std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

			if((cmd == "/add") && ((i + 1) < argc))
			{
				std::string macString = argv[++i];
				
				char macBuffer[6];
				RtlZeroMemory(macBuffer, 6);

				size_t charSep = 0;
				for(int i = 0; i < 6; ++i)
				{
					charSep = macString.find(':', 0);
					macBuffer[i] = static_cast<char>(strtol(macString.substr(0, charSep).c_str(), NULL, 16));
					macString = macString.substr(charSep + 1);
				}

				DWORD dwBytesReturned;
				if(DeviceIoControl(hDevice, IOCTL_ADD_ARP_FILTER, reinterpret_cast<LPVOID>(&macBuffer), sizeof(macBuffer), NULL, NULL, &dwBytesReturned, NULL) == 0)
				{
					throw std::exception("DeviceIoControl(IOCTL_ADD_ARP_FILTER) Failed.");
				}
			}
			else if((cmd == "/remove") && ((i + 1) < argc))
			{
				std::string macString = argv[++i];

				char macBuffer[6];
				RtlZeroMemory(macBuffer, 6);

				size_t charSep = 0;
				for(int i = 0; i < 6; ++i)
				{
					charSep = macString.find(':', 0);
					macBuffer[i] = static_cast<char>(strtol(macString.substr(0, charSep).c_str(), NULL, 16));
					macString = macString.substr(charSep + 1);
				}

				DWORD dwBytesReturned;
				if(DeviceIoControl(hDevice, IOCTL_REM_ARP_FILTER, reinterpret_cast<LPVOID>(&macBuffer), sizeof(macBuffer), NULL, NULL, &dwBytesReturned, NULL) == 0)
				{
					throw std::exception("DeviceIoControl(IOCTL_REM_ARP_FILTER) Failed.");
				}
			}
			else if(cmd == "/list")
			{
				char macBuffer[3072];
				DWORD dwBytesReturned;
				if(DeviceIoControl(hDevice, IOCTL_LST_ARP_FILTER, NULL, NULL, reinterpret_cast<LPVOID>(&macBuffer), sizeof(macBuffer), &dwBytesReturned, NULL) == 0)
				{
					throw std::exception("DeviceIoControl(IOCTL_LST_ARP_FILTER) Failed.");
				}

				std::cout << "Currently Filtering:" << std::endl << std::endl;

				for(DWORD i = 0; i < dwBytesReturned; i += 6)
				{
					std::cout << std::setfill('0')
							  << std::hex
							  << std::setw(2) << static_cast<unsigned short>(macBuffer[i]) << ":"
							  << std::setw(2) << static_cast<unsigned short>(macBuffer[i + 1]) << ":"
							  << std::setw(2) << static_cast<unsigned short>(macBuffer[i + 2]) << ":"
							  << std::setw(2) << static_cast<unsigned short>(macBuffer[i + 3]) << ":"
							  << std::setw(2) << static_cast<unsigned short>(macBuffer[i + 4]) << ":"
							  << std::setw(2) << static_cast<unsigned short>(macBuffer[i + 5])
							  << std::dec << std::endl;
				}
				std::cout << std::endl;
			}
			else
			{
				PrintUsage();
				throw std::exception("Unknown Command.");
			}
		}		
	}
	catch(const std::exception &e)
	{
		std::cout << std::endl << "Error: " << e.what() << std::endl;
	}

	if(hDevice != NULL)
	{
		CloseHandle(hDevice);
		hDevice = NULL;
	}

	return 0;
}

// ========================================================================================================================

void PrintUsage()
{
	std::cout << "Usage: NdisArpFilterCtrl.exe /Add <aa:bb:cc:dd:ee:ff> /Remove <aa:bb:cc:dd:ee:ff> /List"
			  << std::endl << std::endl;
}

// ========================================================================================================================
