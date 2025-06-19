/** @fileAdd commentMore actions
  This file defines the hob structure for EFI System Table Base

  Copyright (c) 2025, NewFW Limited
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef UNIVERSAL_PAYLOAD_EFI_SYSTEM_TABLE_BASE_H_
#define UNIVERSAL_PAYLOAD_EFI_SYSTEM_TABLE_BASE_H_
#include <Uefi.h>
#include <UniversalPayload/UniversalPayload.h>

extern GUID  gUniversalPayloadSystemTableBaseGuid;

#pragma pack (1)
typedef struct {
  UNIVERSAL_PAYLOAD_GENERIC_HEADER    Header;
  EFI_PHYSICAL_ADDRESS    SystemTableBase;
} UNIVERSAL_PAYLOAD_SYSTEM_TABLE_BASE;

#endif //UNIVERSAL_PAYLOAD_EFI_SYSTEM_TABLE_BASE_H_
