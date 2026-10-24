/** @file
  This file handles post bootloader stage operations for RISCV64 architecture.

  Copyright (c) 2026. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseRiscVMmuLib.h>
#include <Library/DebugLib.h>
#include <Register/RiscV64/RiscVEncoding.h>

#include "BlSupportDxe.h"

/**
  Determine whether supervisor address translation is enabled.

  @retval TRUE   SATP mode is not Bare.
  @retval FALSE  SATP mode is Bare.

**/
STATIC
BOOLEAN
RiscVPayloadMmuEnabled (
  VOID
  )
{
  return ((RiscVGetSupervisorAddressTranslationRegister () & SATP64_MODE) !=
          ((UINT64)SATP_MODE_OFF << SATP64_MODE_SHIFT));
}

/**
  Architecture level additional operation which needs to be performed before
  launching payload.

  kexec enters S-mode with SATP in Bare mode. Build an identity map from the
  GCD memory space map and enable the highest SATP mode the hart supports.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.

**/
EFI_STATUS
EFIAPI
BlArchAdditionalOps (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  (VOID)ImageHandle;
  (VOID)SystemTable;

  if (RiscVPayloadMmuEnabled ()) {
    return EFI_SUCCESS;
  }

  Status = RiscVConfigureMmu ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to enable MMU: %r\n", __func__, Status));
    ASSERT_EFI_ERROR (Status);
    return Status;
  }

  return EFI_SUCCESS;
}
