/** @file
  LinuxBoot PEI module include file.
**/
#ifndef _LINUXBOOT_PEI_H_INCLUDED_
#define _LINUXBOOT_PEI_H_INCLUDED_

#if defined(_MSC_VER)
#pragma warning(disable : 4200)
#endif

#pragma pack(1)
typedef struct SerialPortConfigStruct {
  UINT32 Type;
  UINT32 BaseAddr;
  UINT32 Baud;
  UINT32 RegWidth;
  UINT32 InputHertz;
  UINT32 UartPciAddr;
} SerialPortConfig;

typedef struct MemoryMapEntryStruct {
  UINT64 Start;
  UINT64 End;
  UINT32 Type;
} MemoryMapEntry;

typedef struct UefiPayloadConfigStruct {
  UINT64 Version;
  UINT64 AcpiBase;
  UINT64 AcpiSize;
  UINT64 SmbiosBase;
  UINT64 SmbiosSize;
  SerialPortConfig SerialConfig;
  UINT32 NumMemoryMapEntries;
  MemoryMapEntry MemoryMapEntries[0];
} UefiPayloadConfig;
#pragma pack()

#define UEFI_PAYLOAD_CONFIG_VERSION 1

#endif  // _LINUXBOOT_PEI_H_INCLUDED_
