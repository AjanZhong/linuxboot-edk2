/** @file
  Support Libraries of CPU related Library for AArch64 specific services.

  Copyright 2024 Google LLC

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "UefiPayloadEntry.h"

VOID
EFIAPI
InitializeFloatingPointUnits (
  VOID
  )
{
}

UINT8
GetPhyAddrBit (
  VOID
  )
{
  return 44;
}
