/** @file
  This file defines the structure for EFI variable info from device tree.

  Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef UNIVERSAL_PAYLOAD_EFI_VARIABLE_H_
#define UNIVERSAL_PAYLOAD_EFI_VARIABLE_H_

#include <UniversalPayload/UniversalPayload.h>

#define U_ROOT_EFIVAR_MAGIC  "u-root-efivar-v1"

#pragma pack(1)

//
// Structure to store EFI variable data in HOB
// Variable name is stored as ASCII string following this structure
// Variable data is stored following the name
//
typedef struct {
  UNIVERSAL_PAYLOAD_GENERIC_HEADER    Header;
  EFI_GUID                            VariableGuid;
  UINT32                              Attributes;
  UINT32                              NameSize;      // Size of variable name in bytes (ASCII, including null terminator)
  UINT32                              DataSize;      // Size of variable data in bytes
  //
  // Variable name (ASCII, null-terminated) follows immediately after this structure
  // Variable data follows immediately after the name
  //
} UNIVERSAL_PAYLOAD_EFI_VARIABLE;

#pragma pack()

#define UNIVERSAL_PAYLOAD_EFI_VARIABLE_REVISION  1

extern GUID  gUniversalPayloadEfiVariableGuid;

#endif //  UNIVERSAL_PAYLOAD_EFI_VARIABLE_H_

