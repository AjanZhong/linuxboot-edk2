/** @file
  This driver will setup PCDs for DXE phase from HOBs
  and initialise architecture-specific settings and resources.

  Copyright (c) 2014 - 2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include "BlSupportDxe.h"
#include <UniversalPayload/EfiVariable.h>

/**
  Convert ASCII string to Unicode string.

  @param[in]   AsciiString    Pointer to the ASCII string.
  @param[out]  UnicodeString  Pointer to the Unicode string buffer.
  @param[in]   UnicodeSize    Size of the Unicode string buffer in bytes.

  @retval EFI_SUCCESS         The conversion was successful.
  @retval EFI_BUFFER_TOO_SMALL  The Unicode buffer is too small.

**/
EFI_STATUS
AsciiToUnicodeString (
  IN  CONST CHAR8  *AsciiString,
  OUT CHAR16       *UnicodeString,
  IN  UINTN        UnicodeSize
  )
{
  UINTN  Index;
  UINTN  Length;

  if ((AsciiString == NULL) || (UnicodeString == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Length = AsciiStrLen (AsciiString);
  if ((Length + 1) * sizeof (CHAR16) > UnicodeSize) {
    return EFI_BUFFER_TOO_SMALL;
  }

  for (Index = 0; Index <= Length; Index++) {
    UnicodeString[Index] = (CHAR16)AsciiString[Index];
  }

  return EFI_SUCCESS;
}

/**
  Callback function for ExitBootServices event.
  This function restores EFI variables from HOBs.

  @param[in]  Event       Event whose notification function is being invoked.
  @param[in]  Context     Pointer to the notification function's context.

**/
VOID
EFIAPI
OnExitBootServices (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS                      Status;
  EFI_HOB_GUID_TYPE               *GuidHob;
  UNIVERSAL_PAYLOAD_EFI_VARIABLE  *EfiVarHob;
  CHAR8                           *NamePtr;
  UINT8                           *DataPtr;
  CHAR16                          *VariableName;
  UINTN                           VariableNameSize;
  UINTN                           SuccessCount;
  UINTN                           TotalCount;

  DEBUG ((DEBUG_INFO, "BlSupportDxe: ExitBootServices event triggered, restoring variables from HOBs\n"));

  SuccessCount = 0;
  TotalCount = 0;

  // Iterate through all EfiVariable HOBs
  for (GuidHob = GetFirstGuidHob (&gUniversalPayloadEfiVariableGuid);
       GuidHob != NULL;
       GuidHob = GetNextGuidHob (&gUniversalPayloadEfiVariableGuid, GET_NEXT_HOB (GuidHob)))
  {
    TotalCount++;
    EfiVarHob = (UNIVERSAL_PAYLOAD_EFI_VARIABLE *)GET_GUID_HOB_DATA (GuidHob);

    // Get pointers to name and data within the HOB
    NamePtr = (CHAR8 *)(EfiVarHob + 1);
    DataPtr = (UINT8 *)NamePtr + EfiVarHob->NameSize;

    // Convert ASCII name to Unicode
    VariableNameSize = (AsciiStrLen (NamePtr) + 1) * sizeof (CHAR16);
    VariableName = AllocatePool (VariableNameSize);
    if (VariableName == NULL) {
      DEBUG ((DEBUG_ERROR, "BlSupportDxe: Failed to allocate memory for variable name\n"));
      continue;
    }

    Status = AsciiToUnicodeString (NamePtr, VariableName, VariableNameSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "BlSupportDxe: Failed to convert variable name: %r\n", Status));
      FreePool (VariableName);
      continue;
    }

    // Check if variable already exists - if it does with different attributes, delete it first
    {
      UINTN       ExistingDataSize = 0;
      UINT32      ExistingAttributes;
      EFI_STATUS  GetStatus;

      GetStatus = gRT->GetVariable (
                          VariableName,
                          &EfiVarHob->VariableGuid,
                          &ExistingAttributes,
                          &ExistingDataSize,
                          NULL
                          );
      if (!EFI_ERROR (GetStatus)) {
        // Variable exists - check if attributes match
        if ((EfiVarHob->Attributes != 0) && ((EfiVarHob->Attributes & (~EFI_VARIABLE_APPEND_WRITE)) != ExistingAttributes)) {
          DEBUG ((
            DEBUG_INFO,
            "BlSupportDxe: Variable exists with different attributes (0x%x vs 0x%x), deleting first\n",
            ExistingAttributes,
            EfiVarHob->Attributes
            ));
          // Delete the existing variable first
          gRT->SetVariable (
            VariableName,
            &EfiVarHob->VariableGuid,
            0,
            0,
            NULL
            );
        }
      }
    }

    // Set the EFI variable
    Status = gRT->SetVariable (
                    VariableName,
                    &EfiVarHob->VariableGuid,
                    EfiVarHob->Attributes,
                    EfiVarHob->DataSize,
                    DataPtr
                    );

    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "BlSupportDxe: Failed to set variable (Name: %s, GUID: %g, Attr: 0x%x, Size: %d): %r\n",
        VariableName,
        &EfiVarHob->VariableGuid,
        EfiVarHob->Attributes,
        EfiVarHob->DataSize,
        Status
        ));
    } else {
      SuccessCount++;
      DEBUG ((
        DEBUG_INFO,
        "BlSupportDxe: Successfully restored variable (Name: %s, GUID: %g, Attr: 0x%x, Size: %d)\n",
        VariableName,
        &EfiVarHob->VariableGuid,
        EfiVarHob->Attributes,
        EfiVarHob->DataSize
        ));
    }

    FreePool (VariableName);
  }

  if (TotalCount == 0) {
    DEBUG ((DEBUG_INFO, "BlSupportDxe: No EFI variable HOBs found\n"));
  } else {
    DEBUG ((DEBUG_INFO, "BlSupportDxe: Variable restoration completed: %d/%d successful\n", SuccessCount, TotalCount));
  }

  // Close the event since we only need to restore once
  gBS->CloseEvent (Event);
}

/**
  Main entry for the bootloader support DXE module.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
BlDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                 Status;
  EFI_HOB_GUID_TYPE          *GuidHob;
  EFI_PEI_GRAPHICS_INFO_HOB  *GfxInfo;
  ACPI_BOARD_INFO            *AcpiBoardInfo;
  EFI_EVENT                  ExitBootServicesEvent;

  //
  // Find the frame buffer information and update PCDs
  //
  GuidHob = GetFirstGuidHob (&gEfiGraphicsInfoHobGuid);
  if (GuidHob != NULL) {
    GfxInfo = (EFI_PEI_GRAPHICS_INFO_HOB *)GET_GUID_HOB_DATA (GuidHob);
    Status  = PcdSet32S (PcdVideoHorizontalResolution, GfxInfo->GraphicsMode.HorizontalResolution);
    ASSERT_EFI_ERROR (Status);
    Status = PcdSet32S (PcdVideoVerticalResolution, GfxInfo->GraphicsMode.VerticalResolution);
    ASSERT_EFI_ERROR (Status);
    Status = PcdSet32S (PcdSetupVideoHorizontalResolution, GfxInfo->GraphicsMode.HorizontalResolution);
    ASSERT_EFI_ERROR (Status);
    Status = PcdSet32S (PcdSetupVideoVerticalResolution, GfxInfo->GraphicsMode.VerticalResolution);
    ASSERT_EFI_ERROR (Status);
  }

  //
  // Set PcdPciExpressBaseAddress and PcdPciExpressBaseSize by HOB info
  //
  GuidHob = GetFirstGuidHob (&gUefiAcpiBoardInfoGuid);
  if (GuidHob != NULL) {
    AcpiBoardInfo = (ACPI_BOARD_INFO *)GET_GUID_HOB_DATA (GuidHob);
    Status        = PcdSet64S (PcdPciExpressBaseAddress, AcpiBoardInfo->PcieBaseAddress);
    ASSERT_EFI_ERROR (Status);
    Status = PcdSet64S (PcdPciExpressBaseSize, AcpiBoardInfo->PcieBaseSize);
    ASSERT_EFI_ERROR (Status);
  }

  Status = BlArchAdditionalOps (ImageHandle, SystemTable);
  ASSERT_EFI_ERROR (Status);

  //
  // Register ExitBootServices event to restore variables from EfiVariable HOBs
  // Variables are restored at ExitBootServices to avoid write protection issues
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  OnExitBootServices,
                  NULL,
                  &gEfiEventExitBootServicesGuid,
                  &ExitBootServicesEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "BlSupportDxe: Failed to create ExitBootServices event: %r\n", Status));
    return Status;
  }

  return EFI_SUCCESS;
}
