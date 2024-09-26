/** @file
  Support Libraries of CPU related Library for X64 specific services.

  Copyright 2024 Google LLC

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "UefiPayloadEntry.h"

UINT8
GetPhyAddrBit (
  VOID
  )
{
  UINT32  RegEax;

  AsmCpuid (0x80000000, &RegEax, NULL, NULL, NULL);
  if (RegEax >= 0x80000008) {
    AsmCpuid (0x80000008, &RegEax, NULL, NULL, NULL);
    return (UINT8)RegEax;
  }

  return 36;
}
