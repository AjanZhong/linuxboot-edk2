/** @file
  This driver will setup PCDs for DXE phase from HOBs
  and initialise architecture-specific settings and resources.

  Copyright (c) 2014 - 2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include "BlSupportDxe.h"

STATIC BOOLEAN  mVariableRestoreDone = FALSE;

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
  Parse a device tree node and restore the EFI variable if it contains
  u-root EFI variable information.

  @param[in]  FdtBase    Pointer to the device tree base.
  @param[in]  Node       Device tree node offset.

  @retval EFI_SUCCESS    The variable was successfully restored or skipped.
  @retval Other          Error occurred during parsing or restoration.

**/
EFI_STATUS
ParseEfiVariableNode (
  IN VOID   *FdtBase,
  IN INT32  Node
  )
{
  EFI_STATUS        Status;
  CONST FDT_PROPERTY *PropertyPtr;
  INT32             TempLen;
  CONST CHAR8       *MagicStr;
  CONST CHAR8       *NameStr;
  CONST CHAR8       *GuidStr;
  CHAR8             *TempNameStr;
  CHAR8             *TempGuidStr;
  UINT32            *AttributesPtr;
  UINT32            Attributes;
  CONST UINT8       *DataPtr;
  UINT32            DataSize;
  EFI_GUID          VariableGuid;
  CHAR16            *VariableName;
  UINTN             VariableNameSize;
  UINT8             *VariableData;

  // Check for "magic" property
  PropertyPtr = FdtGetProperty (FdtBase, Node, "magic", &TempLen);
  if (PropertyPtr == NULL) {
    return EFI_NOT_FOUND;
  }

  // Check magic string - device tree properties typically include null terminator
  // So length can be either strlen(magic) or strlen(magic) + 1
  MagicStr = (CONST CHAR8 *)PropertyPtr->Data;
  {
    UINTN  MagicLen = AsciiStrLen (U_ROOT_EFIVAR_MAGIC);
    // Allow for null terminator in device tree property
    if ((TempLen != MagicLen) && (TempLen != (MagicLen + 1))) {
      return EFI_NOT_FOUND;
    }
    // Compare up to the expected length (ignore null terminator if present)
    if (AsciiStrnCmp (MagicStr, U_ROOT_EFIVAR_MAGIC, MagicLen) != 0) {
      return EFI_NOT_FOUND;
    }
  }

  //DEBUG ((DEBUG_INFO, "BlSupportDxe: Found u-root EFI variable node\n"));
  // Get variable name
  PropertyPtr = FdtGetProperty (FdtBase, Node, "name", &TempLen);
  if (PropertyPtr == NULL) {
    DEBUG ((DEBUG_ERROR, "BlSupportDxe: Missing 'name' property\n"));
    return EFI_INVALID_PARAMETER;
  }

  // Device tree properties may not be null-terminated, so we need to handle that
  // Allocate a temporary buffer for null-terminated string
  NameStr     = (CONST CHAR8 *)PropertyPtr->Data;
  TempNameStr = NULL;
  if (TempLen == 0 || (TempLen > 0 && NameStr[TempLen - 1] != '\0')) {
    // Need to add null terminator
    TempNameStr = AllocatePool (TempLen + 1);
    if (TempNameStr == NULL) {
      DEBUG ((DEBUG_ERROR, "BlSupportDxe: Failed to allocate memory for name string\n"));
      return EFI_OUT_OF_RESOURCES;
    }
    CopyMem (TempNameStr, NameStr, TempLen);
    TempNameStr[TempLen] = '\0';
    NameStr = TempNameStr;
  }

  VariableNameSize = (AsciiStrLen (NameStr) + 1) * sizeof (CHAR16);
  VariableName = AllocatePool (VariableNameSize);
  if (VariableName == NULL) {
    DEBUG ((DEBUG_ERROR, "BlSupportDxe: Failed to allocate memory for variable name\n"));
    if (TempNameStr != NULL) {
      FreePool (TempNameStr);
    }
    return EFI_OUT_OF_RESOURCES;
  }

  // Convert ASCII to Unicode manually
  {
    UINTN  Index;
    for (Index = 0; NameStr[Index] != '\0'; Index++) {
      VariableName[Index] = (CHAR16)NameStr[Index];
    }
    VariableName[Index] = L'\0';
  }

  // Get GUID
  PropertyPtr = FdtGetProperty (FdtBase, Node, "guid", &TempLen);
  if (PropertyPtr == NULL) {
    DEBUG ((DEBUG_ERROR, "BlSupportDxe: Missing 'guid' property\n"));
    FreePool (VariableName);
    if (TempNameStr != NULL) {
      FreePool (TempNameStr);
    }
    return EFI_INVALID_PARAMETER;
  }

  // GUID string must be null-terminated and exactly 36 characters
  GuidStr     = (CONST CHAR8 *)PropertyPtr->Data;
  TempGuidStr = NULL;
  if (TempLen < 36 || (TempLen >= 36 && GuidStr[36] != '\0')) {
    // Need to ensure null termination
    TempGuidStr = AllocatePool (37); // 36 chars + null terminator
    if (TempGuidStr == NULL) {
      DEBUG ((DEBUG_ERROR, "BlSupportDxe: Failed to allocate memory for GUID string\n"));
      FreePool (VariableName);
      if (TempNameStr != NULL) {
        FreePool (TempNameStr);
      }
      return EFI_OUT_OF_RESOURCES;
    }
    CopyMem (TempGuidStr, GuidStr, (TempLen < 36) ? TempLen : 36);
    TempGuidStr[36] = '\0';
    GuidStr = TempGuidStr;
  }

  Status = AsciiStrToGuid (GuidStr, &VariableGuid);
  if (RETURN_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "BlSupportDxe: Invalid GUID format: %a\n", GuidStr));
    FreePool (VariableName);
    if (TempNameStr != NULL) {
      FreePool (TempNameStr);
    }
    if (TempGuidStr != NULL) {
      FreePool (TempGuidStr);
    }
    return EFI_INVALID_PARAMETER;
  }

  // Get attributes
  PropertyPtr = FdtGetProperty (FdtBase, Node, "attributes", &TempLen);
  if (PropertyPtr == NULL) {
    DEBUG ((DEBUG_ERROR, "BlSupportDxe: Missing 'attributes' property\n"));
    FreePool (VariableName);
    return EFI_INVALID_PARAMETER;
  }

  AttributesPtr = (UINT32 *)PropertyPtr->Data;
  Attributes    = Fdt32ToCpu (*AttributesPtr);

  // Get data
  PropertyPtr = FdtGetProperty (FdtBase, Node, "data", &TempLen);
  if (PropertyPtr == NULL) {
    DEBUG ((DEBUG_ERROR, "BlSupportDxe: Missing 'data' property\n"));
    FreePool (VariableName);
    return EFI_INVALID_PARAMETER;
  }

  DataPtr  = (CONST UINT8 *)PropertyPtr->Data;
  DataSize = TempLen;

  // Allocate buffer for variable data
  VariableData = AllocatePool (DataSize);
  if (VariableData == NULL) {
    DEBUG ((DEBUG_ERROR, "BlSupportDxe: Failed to allocate memory for variable data\n"));
    FreePool (VariableName);
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem (VariableData, DataPtr, DataSize);

  // Check if variable already exists - if it does with different attributes, delete it first
  {
    UINTN       ExistingDataSize = 0;
    UINT32      ExistingAttributes;
    EFI_STATUS  GetStatus;

    GetStatus = gRT->GetVariable (
                        VariableName,
                        &VariableGuid,
                        &ExistingAttributes,
                        &ExistingDataSize,
                        NULL
                        );
    if (!EFI_ERROR (GetStatus)) {
      // Variable exists - check if attributes match
      if ((Attributes != 0) && ((Attributes & (~EFI_VARIABLE_APPEND_WRITE)) != ExistingAttributes)) {
        DEBUG ((
          DEBUG_INFO,
          "BlSupportDxe: Variable %a exists with different attributes (0x%x vs 0x%x), deleting first\n",
          NameStr,
          ExistingAttributes,
          Attributes
          ));
        // Delete the existing variable first
        gRT->SetVariable (
          VariableName,
          &VariableGuid,
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
                  &VariableGuid,
                  Attributes,
                  DataSize,
                  VariableData
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "BlSupportDxe: Failed to set variable %a (GUID: %g, Attr: 0x%x, Size: %d): %r\n",
      NameStr,
      &VariableGuid,
      Attributes,
      DataSize,
      Status
      ));
  } else {
    DEBUG ((
      DEBUG_INFO,
      "BlSupportDxe: Successfully restored variable %a (GUID: %g, Attr: 0x%x, Size: %d)\n",
      NameStr,
      &VariableGuid,
      Attributes,
      DataSize
      ));
  }

  // Clean up
  FreePool (VariableName);
  FreePool (VariableData);
  if (TempNameStr != NULL) {
    FreePool (TempNameStr);
  }
  if (TempGuidStr != NULL) {
    FreePool (TempGuidStr);
  }

  return Status;
}

/**
  Parse all device tree nodes and restore EFI variables from u-root.

  @param[in]  FdtBase    Pointer to the device tree base.

  @retval EFI_SUCCESS    All variables were processed (some may have failed).
  @retval Other          Error occurred during parsing.

**/
EFI_STATUS
ParseDeviceTreeForVariables (
  IN VOID  *FdtBase
  )
{
  INT32                Node;
  INT32                Depth;
  EFI_STATUS           Status;
  UINTN                VariableCount;
  CONST FDT_PROPERTY   *PropertyPtr;
  INT32                TempLen;

  if (FdtBase == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Validate FDT header
  if (FdtCheckHeader (FdtBase) != 0) {
    DEBUG ((DEBUG_ERROR, "BlSupportDxe: Invalid FDT header\n"));
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "BlSupportDxe: Starting device tree parsing for EFI variables\n"));
  DEBUG ((DEBUG_INFO, "BlSupportDxe: Device tree base: 0x%p\n", FdtBase));

  VariableCount = 0;

  // Walk through all nodes in the device tree
  Depth = 0;
  for (Node = FdtNextNode (FdtBase, 0, &Depth); Node >= 0; Node = FdtNextNode (FdtBase, Node, &Depth)) {
    // Check if this node has a "magic" property
    PropertyPtr = FdtGetProperty (FdtBase, Node, "magic", &TempLen);
    if (PropertyPtr != NULL) {
      DEBUG ((DEBUG_INFO, "BlSupportDxe: Found node with magic property (len=%d)\n", TempLen));
    }
    Status = ParseEfiVariableNode (FdtBase, Node);
    if (Status == EFI_SUCCESS) {
      VariableCount++;
    } else if (Status != EFI_NOT_FOUND) {
      // Log errors other than "not found" (which is expected for most nodes)
      DEBUG ((DEBUG_WARN, "BlSupportDxe: Error parsing node: %r\n", Status));
    }
    // Continue parsing other nodes even if one fails
  }

  DEBUG ((DEBUG_INFO, "BlSupportDxe: Restored %d EFI variables from device tree\n", VariableCount));

  return EFI_SUCCESS;
}

/**
  Callback function for ExitBootServices event.
  This function restores EFI variables from u-root device tree nodes.

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
  EFI_HOB_GUID_TYPE              *GuidHob;
  UNIVERSAL_PAYLOAD_DEVICE_TREE  *DeviceTree;
  VOID                           *FdtBase;
  EFI_STATUS                     Status;

  if (mVariableRestoreDone) {
    return;
  }

  DEBUG ((DEBUG_INFO, "BlSupportDxe: ExitBootServices event triggered, restoring variables from device tree\n"));

  // Get device tree from HOB
  GuidHob = GetFirstGuidHob (&gUniversalPayloadDeviceTreeGuid);
  if (GuidHob == NULL) {
    DEBUG ((DEBUG_WARN, "BlSupportDxe: Device tree HOB not found\n"));
    return;
  }

  DeviceTree = (UNIVERSAL_PAYLOAD_DEVICE_TREE *)GET_GUID_HOB_DATA (GuidHob);
  if (DeviceTree == NULL) {
    DEBUG ((DEBUG_ERROR, "BlSupportDxe: Invalid device tree HOB data\n"));
    return;
  }

  FdtBase = (VOID *)(UINTN)DeviceTree->DeviceTreeAddress;
  if (FdtBase == NULL) {
    DEBUG ((DEBUG_ERROR, "BlSupportDxe: Invalid device tree address\n"));
    return;
  }

  DEBUG ((DEBUG_INFO, "BlSupportDxe: Device tree found at 0x%p\n", FdtBase));

  // Parse device tree and restore variables
  Status = ParseDeviceTreeForVariables (FdtBase);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "BlSupportDxe: Failed to parse device tree: %r\n", Status));
    return;
  }

  mVariableRestoreDone = TRUE;
  DEBUG ((DEBUG_INFO, "BlSupportDxe: Variable restoration completed\n"));

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
  // Register ExitBootServices event to restore variables from device tree
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
