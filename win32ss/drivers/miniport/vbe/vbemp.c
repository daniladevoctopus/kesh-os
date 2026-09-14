/*
 * ReactOS VBE miniport video driver
 * Copyright (C) 2004 Filip Navara
 *
 * Power Management and VBE 1.2 support
 * Copyright (C) 2004 Magnus Olsen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * TODO:
 * - Check input parameters everywhere.
 * - Call VideoPortVerifyAccessRanges to reserve the memory we're about
 *   to map.
 */

/* INCLUDES *******************************************************************/

#include "vbemp.h"

#include <devioctl.h>
#include <stdio.h>

#pragma pack(push, 4)
typedef struct _VBE_FRAMEBUF_DATA
{
    USHORT Version;
    USHORT Revision;
    ULONG VideoClock;
    ULONG FrameBufferOffset;
    ULONG ScreenWidth;
    ULONG ScreenHeight;
    ULONG PixelsPerScanLine;
    ULONG BitsPerPixel;
    struct
    {
        ULONG RedMask;
        ULONG GreenMask;
        ULONG BlueMask;
        ULONG ReservedMask;
    } PixelMasks;
} VBE_FRAMEBUF_DATA, *PVBE_FRAMEBUF_DATA;

typedef struct _VBE_PARTIAL_DESC
{
    UCHAR Type;
    UCHAR ShareDisposition;
    USHORT Flags;
    union {
        struct {
            PHYSICAL_ADDRESS Start;
            ULONG Length;
        } Memory;
        struct {
            ULONG DataSize;
            ULONG Reserved1;
            ULONG Reserved2;
        } DeviceSpecificData;
    } u;
} VBE_PARTIAL_DESC;

typedef struct _VBE_PARTIAL_LIST
{
    USHORT Version;
    USHORT Revision;
    ULONG Count;
    VBE_PARTIAL_DESC PartialDescriptors[2];
} VBE_PARTIAL_LIST;

typedef struct _VBE_FULL_DESC
{
    ULONG InterfaceType;
    ULONG BusNumber;
    VBE_PARTIAL_LIST PartialResourceList;
} VBE_FULL_DESC;
#pragma pack(pop)

#ifndef KEY_READ
#define KEY_READ 0x00020019
#endif

typedef ULONG ACCESS_MASK;

typedef enum _KEY_VALUE_INFORMATION_CLASS {
    KeyValueBasicInformation,
    KeyValueFullInformation,
    KeyValuePartialInformation
} KEY_VALUE_INFORMATION_CLASS;

typedef struct _KEY_VALUE_FULL_INFORMATION {
    ULONG TitleIndex;
    ULONG Type;
    ULONG DataOffset;
    ULONG DataLength;
    ULONG NameLength;
    WCHAR Name[1];
} KEY_VALUE_FULL_INFORMATION, *PKEY_VALUE_FULL_INFORMATION;

NTSYSAPI
NTSTATUS
NTAPI
ZwOpenKey(
    _Out_ PHANDLE KeyHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ POBJECT_ATTRIBUTES ObjectAttributes);

NTSYSAPI
NTSTATUS
NTAPI
ZwQueryValueKey(
    _In_ HANDLE KeyHandle,
    _In_ PUNICODE_STRING ValueName,
    _In_ KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
    _Out_writes_bytes_to_opt_(Length, *ResultLength) PVOID KeyValueInformation,
    _In_ ULONG Length,
    _Out_ PULONG ResultLength);

NTSYSAPI
NTSTATUS
NTAPI
ZwClose(
    _In_ HANDLE Handle);

NTSYSAPI
VOID
NTAPI
RtlInitUnicodeString(
    _Out_ PUNICODE_STRING DestinationString,
    _In_opt_z_ PCWSTR SourceString);

static BOOLEAN
ParseFramebufferData(
    _In_ PVOID Data,
    _In_ ULONG DataLength,
    _Out_ PPHYSICAL_ADDRESS VideoRamAddress,
    _Out_ PULONG VideoRamSize,
    _Out_ PVBE_FRAMEBUF_DATA VideoConfigData)
{
    ULONG Offset;

    if (DataLength >= sizeof(VBE_FULL_DESC))
    {
        VBE_FULL_DESC *FullDesc = (VBE_FULL_DESC *)Data;
        VBE_PARTIAL_LIST *PartList = &FullDesc->PartialResourceList;
        if (PartList->Count >= 2 &&
            PartList->PartialDescriptors[0].Type == 3 /* CmResourceTypeMemory */ &&
            PartList->PartialDescriptors[1].Type == 5 /* CmResourceTypeDeviceSpecific */)
        {
            VBE_FRAMEBUF_DATA *FbData = (VBE_FRAMEBUF_DATA *)(&PartList->PartialDescriptors[1] + 1);
            if (FbData->ScreenWidth >= 320 && FbData->ScreenWidth <= 7680 &&
                FbData->ScreenHeight >= 200 && FbData->ScreenHeight <= 4320 &&
                FbData->BitsPerPixel >= 8 && FbData->BitsPerPixel <= 32)
            {
                VideoRamAddress->QuadPart = PartList->PartialDescriptors[0].u.Memory.Start.QuadPart;
                *VideoRamSize = PartList->PartialDescriptors[0].u.Memory.Length;
                *VideoConfigData = *FbData;
                return TRUE;
            }
        }
    }

    /* Fallback: scan for VBE_FRAMEBUF_DATA signature inside the buffer */
    if (DataLength >= sizeof(VBE_FRAMEBUF_DATA) + 8)
    {
        for (Offset = 0; Offset + sizeof(VBE_FRAMEBUF_DATA) <= DataLength; Offset += 4)
        {
            PVBE_FRAMEBUF_DATA Candidate = (PVBE_FRAMEBUF_DATA)((PUCHAR)Data + Offset);
            if (Candidate->Version == 1 &&
                Candidate->ScreenWidth >= 320 && Candidate->ScreenWidth <= 7680 &&
                Candidate->ScreenHeight >= 200 && Candidate->ScreenHeight <= 4320 &&
                (Candidate->BitsPerPixel == 32 || Candidate->BitsPerPixel == 24 || Candidate->BitsPerPixel == 16))
            {
                ULONG MemOffset;
                for (MemOffset = 0; MemOffset + 16 <= Offset; MemOffset += 4)
                {
                    VBE_PARTIAL_DESC *MemDesc = (VBE_PARTIAL_DESC *)((PUCHAR)Data + MemOffset);
                    if (MemDesc->Type == 3 && MemDesc->u.Memory.Start.QuadPart != 0)
                    {
                        VideoRamAddress->QuadPart = MemDesc->u.Memory.Start.QuadPart;
                        *VideoRamSize = MemDesc->u.Memory.Length;
                        *VideoConfigData = *Candidate;
                        return TRUE;
                    }
                }
                VideoRamAddress->QuadPart = 0;
                *VideoRamSize = Candidate->ScreenWidth * Candidate->ScreenHeight * (Candidate->BitsPerPixel / 8);
                *VideoConfigData = *Candidate;
                return TRUE;
            }
        }
    }
    return FALSE;
}

static BOOLEAN
FindBootDisplayFromRegistry(
    _Out_ PPHYSICAL_ADDRESS VideoRamAddress,
    _Out_ PULONG VideoRamSize,
    _Out_ PVBE_FRAMEBUF_DATA VideoConfigData)
{
    OBJECT_ATTRIBUTES ObjectAttributes;
    UNICODE_STRING KeyName;
    HANDLE KeyHandle;
    NTSTATUS Status;
    WCHAR PathBuffer[256];
    ULONG AdapterIndex, CtrlIndex;

    for (AdapterIndex = 0; AdapterIndex < 16; AdapterIndex++)
    {
        for (CtrlIndex = 0; CtrlIndex < 4; CtrlIndex++)
        {
            swprintf(PathBuffer,
                     sizeof(PathBuffer) / sizeof(WCHAR),
                     L"\\Registry\\Machine\\HARDWARE\\DESCRIPTION\\System\\MultifunctionAdapter\\%lu\\DisplayController\\%lu",
                     AdapterIndex, CtrlIndex);
            RtlInitUnicodeString(&KeyName, PathBuffer);
            InitializeObjectAttributes(&ObjectAttributes, &KeyName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
            Status = ZwOpenKey(&KeyHandle, KEY_READ, &ObjectAttributes);
            if (NT_SUCCESS(Status))
            {
                UCHAR Buffer[512];
                PKEY_VALUE_FULL_INFORMATION ValueInfo = (PKEY_VALUE_FULL_INFORMATION)Buffer;
                ULONG ResultLength;
                UNICODE_STRING ValName;

                RtlInitUnicodeString(&ValName, L"Configuration Data");
                Status = ZwQueryValueKey(KeyHandle, &ValName, KeyValueFullInformation, Buffer, sizeof(Buffer), &ResultLength);
                ZwClose(KeyHandle);

                if (NT_SUCCESS(Status) && ValueInfo->DataLength > 0)
                {
                    PVOID Data = (PVOID)((ULONG_PTR)ValueInfo + ValueInfo->DataOffset);
                    if (ParseFramebufferData(Data, ValueInfo->DataLength, VideoRamAddress, VideoRamSize, VideoConfigData))
                    {
                        return TRUE;
                    }
                }
            }
        }
    }

    for (CtrlIndex = 0; CtrlIndex < 4; CtrlIndex++)
    {
        swprintf(PathBuffer,
                 sizeof(PathBuffer) / sizeof(WCHAR),
                 L"\\Registry\\Machine\\HARDWARE\\DESCRIPTION\\System\\DisplayController\\%lu",
                 CtrlIndex);
        RtlInitUnicodeString(&KeyName, PathBuffer);
        InitializeObjectAttributes(&ObjectAttributes, &KeyName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
        Status = ZwOpenKey(&KeyHandle, KEY_READ, &ObjectAttributes);
        if (NT_SUCCESS(Status))
        {
            UCHAR Buffer[512];
            PKEY_VALUE_FULL_INFORMATION ValueInfo = (PKEY_VALUE_FULL_INFORMATION)Buffer;
            ULONG ResultLength;
            UNICODE_STRING ValName;

            RtlInitUnicodeString(&ValName, L"Configuration Data");
            Status = ZwQueryValueKey(KeyHandle, &ValName, KeyValueFullInformation, Buffer, sizeof(Buffer), &ResultLength);
            ZwClose(KeyHandle);

            if (NT_SUCCESS(Status) && ValueInfo->DataLength > 0)
            {
                PVOID Data = (PVOID)((ULONG_PTR)ValueInfo + ValueInfo->DataOffset);
                if (ParseFramebufferData(Data, ValueInfo->DataLength, VideoRamAddress, VideoRamSize, VideoConfigData))
                {
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}

static BOOLEAN
VBEFindBootDisplay(
    _Out_ PPHYSICAL_ADDRESS VramAddress,
    _Out_ PULONG VramSize,
    _Out_ PVBE_FRAMEBUF_DATA VideoConfigData)
{
    return FindBootDisplayFromRegistry(VramAddress, VramSize, VideoConfigData);
}

static BOOLEAN
VBEInitializeUefiGop(
    PVOID HwDeviceExtension,
    PPHYSICAL_ADDRESS VramAddress,
    ULONG VramSize,
    PVBE_FRAMEBUF_DATA VideoConfigData)
{
    PVBE_DEVICE_EXTENSION VBEDeviceExtension = (PVBE_DEVICE_EXTENSION)HwDeviceExtension;
    PVBE_MODEINFO ModeInfo;
    ULONG BytesPerPixel;
    ULONG Stride;
    ULONG i;

    VBEDeviceExtension->VbeInfo.Version = 0x0300;
    VBEDeviceExtension->VbeInfo.TotalMemory = (USHORT)(VramSize / 65536);
    if (VBEDeviceExtension->VbeInfo.TotalMemory == 0)
    {
        VBEDeviceExtension->VbeInfo.TotalMemory = 256; /* 16 MB */
    }
    VideoPortMoveMemory(VBEDeviceExtension->VbeInfo.Signature, "VESA", 4);

    VBEDeviceExtension->ModeCount = 4;
    VBEDeviceExtension->ModeInfo = VideoPortAllocatePool(HwDeviceExtension, VpPagedPool, sizeof(VBE_MODEINFO) * 4, TAG_VBE);
    VBEDeviceExtension->ModeNumbers = VideoPortAllocatePool(HwDeviceExtension, VpPagedPool, sizeof(USHORT) * 4, TAG_VBE);

    if (!VBEDeviceExtension->ModeInfo || !VBEDeviceExtension->ModeNumbers)
    {
        VideoPortDebugPrint(Error, "VBEMP: Failed to allocate mode pool for UEFI GOP\n");
        return FALSE;
    }

    VideoPortZeroMemory(VBEDeviceExtension->ModeInfo, sizeof(VBE_MODEINFO) * 4);

    for (i = 0; i < 4; i++)
    {
        ULONG Width, Height;
        VBEDeviceExtension->ModeNumbers[i] = 0x9990 + (USHORT)i;
        ModeInfo = &VBEDeviceExtension->ModeInfo[i];

        if (i == 0)
        {
            Width = VideoConfigData->ScreenWidth ? VideoConfigData->ScreenWidth : 1024;
            Height = VideoConfigData->ScreenHeight ? VideoConfigData->ScreenHeight : 768;
        }
        else if (i == 1)
        {
            Width = 1024; Height = 768;
        }
        else if (i == 2)
        {
            Width = 800; Height = 600;
        }
        else
        {
            Width = 1280; Height = 720;
        }

        ModeInfo->ModeAttributes = VBE_MODEATTR_LINEAR;
        ModeInfo->XResolution = (USHORT)Width;
        ModeInfo->YResolution = (USHORT)Height;
        ModeInfo->BitsPerPixel = 32;
        ModeInfo->NumberOfPlanes = 1;
        ModeInfo->MemoryModel = VBE_MEMORYMODEL_DIRECTCOLOR;
        ModeInfo->PhysBasePtr = (ULONG)(VramAddress->QuadPart + VideoConfigData->FrameBufferOffset);

        BytesPerPixel = 4;
        Stride = Width * BytesPerPixel;

        ModeInfo->BytesPerScanLine = (USHORT)Stride;
        ModeInfo->LinBytesPerScanLine = (USHORT)Stride;

        ModeInfo->RedMaskSize = 8;
        ModeInfo->GreenMaskSize = 8;
        ModeInfo->BlueMaskSize = 8;
        ModeInfo->ReservedMaskSize = 8;

        ModeInfo->LinRedMaskSize = 8;
        ModeInfo->LinGreenMaskSize = 8;
        ModeInfo->LinBlueMaskSize = 8;
        ModeInfo->LinReservedMaskSize = 8;

        ModeInfo->LinRedFieldPosition = 16;
        ModeInfo->LinGreenFieldPosition = 8;
        ModeInfo->LinBlueFieldPosition = 0;
        ModeInfo->LinReservedFieldPosition = 24;

        ModeInfo->RedFieldPosition = ModeInfo->LinRedFieldPosition;
        ModeInfo->GreenFieldPosition = ModeInfo->LinGreenFieldPosition;
        ModeInfo->BlueFieldPosition = ModeInfo->LinBlueFieldPosition;
        ModeInfo->ReservedFieldPosition = 24;
    }

    VBEDeviceExtension->CurrentMode = 0;

    VideoPortEnumerateChildren(HwDeviceExtension, NULL);

    VideoPortDebugPrint(Info, "VBEMP: UEFI GOP Framebuffer initialized successfully\n");

    return TRUE;
}

#undef LOWORD
#undef HIWORD
#define LOWORD(l)	((USHORT)((ULONG_PTR)(l)))
#define HIWORD(l)	((USHORT)(((ULONG_PTR)(l)>>16)&0xFFFF))

VIDEO_ACCESS_RANGE VBEAccessRange[] =
{
    { {{0x3b0}}, 0x3bb - 0x3b0 + 1, 1, 1, 0 },
    { {{0x3c0}}, 0x3df - 0x3c0 + 1, 1, 1, 0 },
    { {{0xa0000}}, 0x20000, 0, 1, 0 },
};

/* PUBLIC AND PRIVATE FUNCTIONS ***********************************************/

ULONG NTAPI
DriverEntry(IN PVOID Context1, IN PVOID Context2)
{
   VIDEO_HW_INITIALIZATION_DATA InitData;

   VideoPortZeroMemory(&InitData, sizeof(InitData));
   InitData.HwInitDataSize = sizeof(VIDEO_HW_INITIALIZATION_DATA);
   InitData.HwFindAdapter = VBEFindAdapter;
   InitData.HwInitialize = VBEInitialize;
   InitData.HwStartIO = VBEStartIO;
   InitData.HwResetHw = VBEResetHw;
   InitData.HwGetPowerState = VBEGetPowerState;
   InitData.HwSetPowerState = VBESetPowerState;
   InitData.HwDeviceExtensionSize = sizeof(VBE_DEVICE_EXTENSION);
   InitData.HwLegacyResourceList = VBEAccessRange;
   InitData.HwLegacyResourceCount = ARRAYSIZE(VBEAccessRange);
   InitData.AdapterInterfaceType = Isa;

   return VideoPortInitialize(Context1, Context2, &InitData, (PVOID)1);
}

/*
 * VBEFindAdapter
 *
 * Should detect a VBE compatible display adapter, but it's not possible
 * to use video port Int 10 services at this time during initialization,
 * so we always return NO_ERROR and do the real work in VBEInitialize.
 */

VP_STATUS NTAPI
VBEFindAdapter(
   IN PVOID HwDeviceExtension,
   IN PVOID HwContext,
   IN PWSTR ArgumentString,
   IN OUT PVIDEO_PORT_CONFIG_INFO ConfigInfo,
   OUT PUCHAR Again)
{
   if (VideoPortIsNoVesa())
       return ERROR_DEV_NOT_EXIST;

   if (ConfigInfo->Length < sizeof(VIDEO_PORT_CONFIG_INFO))
       return ERROR_INVALID_PARAMETER;

   ConfigInfo->VdmPhysicalVideoMemoryAddress = VBEAccessRange[2].RangeStart;
   ConfigInfo->VdmPhysicalVideoMemoryLength = VBEAccessRange[2].RangeLength;
   return NO_ERROR;
}

/*
 * VBESortModesCallback
 *
 * Helper function for sorting video mode list.
 */

static int
VBESortModesCallback(PVBE_MODEINFO VbeModeInfoA, PVBE_MODEINFO VbeModeInfoB)
{
   /*
    * FIXME: Until some reasonable method for changing video modes will
    * be available we favor more bits per pixel. It should be changed
    * later.
    */
   if (VbeModeInfoA->BitsPerPixel < VbeModeInfoB->BitsPerPixel) return -1;
   if (VbeModeInfoA->BitsPerPixel > VbeModeInfoB->BitsPerPixel) return 1;
   if (VbeModeInfoA->XResolution < VbeModeInfoB->XResolution) return -1;
   if (VbeModeInfoA->XResolution > VbeModeInfoB->XResolution) return 1;
   if (VbeModeInfoA->YResolution < VbeModeInfoB->YResolution) return -1;
   if (VbeModeInfoA->YResolution > VbeModeInfoB->YResolution) return 1;
   return 0;
}

/*
 * VBESortModes
 *
 * Simple function for sorting the video mode list. Uses bubble sort.
 */

VOID FASTCALL
VBESortModes(PVBE_DEVICE_EXTENSION DeviceExtension)
{
   BOOLEAN Finished = FALSE;
   ULONG Pos;
   int Result;
   VBE_MODEINFO TempModeInfo;
   USHORT TempModeNumber;

   while (!Finished)
   {
      Finished = TRUE;
      for (Pos = 0; Pos < DeviceExtension->ModeCount - 1; Pos++)
      {
         Result = VBESortModesCallback(
            DeviceExtension->ModeInfo + Pos,
            DeviceExtension->ModeInfo + Pos + 1);
         if (Result > 0)
         {
            Finished = FALSE;

            VideoPortMoveMemory(
               &TempModeInfo,
               DeviceExtension->ModeInfo + Pos,
               sizeof(VBE_MODEINFO));
            TempModeNumber = DeviceExtension->ModeNumbers[Pos];

            VideoPortMoveMemory(
               DeviceExtension->ModeInfo + Pos,
               DeviceExtension->ModeInfo + Pos + 1,
               sizeof(VBE_MODEINFO));
            DeviceExtension->ModeNumbers[Pos] =
               DeviceExtension->ModeNumbers[Pos + 1];

            VideoPortMoveMemory(
               DeviceExtension->ModeInfo + Pos + 1,
               &TempModeInfo,
               sizeof(VBE_MODEINFO));
            DeviceExtension->ModeNumbers[Pos + 1] = TempModeNumber;
         }
      }
   }
}

/*
 * VBEInitialize
 *
 * Performs the first initialization of the adapter, after the HAL has given
 * up control of the video hardware to the video port driver.
 *
 * This function performs these steps:
 * - Gets global VBE information and finds if VBE BIOS is present.
 * - Builds the internal mode list using the list of modes provided by
 *   the VBE.
 */

BOOLEAN NTAPI
VBEInitialize(PVOID HwDeviceExtension)
{
   INT10_BIOS_ARGUMENTS BiosRegisters;
   VP_STATUS Status;
   PVBE_DEVICE_EXTENSION VBEDeviceExtension =
     (PVBE_DEVICE_EXTENSION)HwDeviceExtension;
   ULONG Length;
   ULONG ModeCount;
   ULONG SuitableModeCount;
   USHORT ModeTemp;
   ULONG CurrentMode;
   PVBE_MODEINFO VbeModeInfo;
   BOOLEAN VbeFound = FALSE;

   if (VideoPortIsNoVesa())
   {
      VBEDeviceExtension->Int10Interface.Version = 0;
      VBEDeviceExtension->Int10Interface.Size = 0;
      return FALSE;
   }

   /*
    * Get the Int 10 interface that we will use for allocating real
    * mode memory and calling the video BIOS.
    */

   VBEDeviceExtension->Int10Interface.Version = VIDEO_PORT_INT10_INTERFACE_VERSION_1;
   VBEDeviceExtension->Int10Interface.Size = sizeof(VIDEO_PORT_INT10_INTERFACE);
   Status = VideoPortQueryServices(
      HwDeviceExtension,
      VideoPortServicesInt10,
      (PINTERFACE)&VBEDeviceExtension->Int10Interface);

   VideoPortDebugPrint(Error, "KESHOOS VBEMP: VideoPortQueryServices(Int10) = 0x%lx, CallBios = %p\n",
                       Status, VBEDeviceExtension->Int10Interface.Int10CallBios);

   if (Status == NO_ERROR && VBEDeviceExtension->Int10Interface.Int10CallBios != NULL)
   {
      Length = 0x400;
      Status = VBEDeviceExtension->Int10Interface.Int10AllocateBuffer(
         VBEDeviceExtension->Int10Interface.Context,
         &VBEDeviceExtension->TrampolineMemorySegment,
         &VBEDeviceExtension->TrampolineMemoryOffset,
         &Length);

      if (Status == NO_ERROR)
      {
         VBEDeviceExtension->Int10Interface.Int10WriteMemory(
            VBEDeviceExtension->Int10Interface.Context,
            VBEDeviceExtension->TrampolineMemorySegment,
            VBEDeviceExtension->TrampolineMemoryOffset,
            "VBE2",
            4);

         VideoPortZeroMemory(&BiosRegisters, sizeof(BiosRegisters));
         BiosRegisters.Eax = VBE_GET_CONTROLLER_INFORMATION;
         BiosRegisters.Edi = VBEDeviceExtension->TrampolineMemoryOffset;
         BiosRegisters.SegEs = VBEDeviceExtension->TrampolineMemorySegment;
         VBEDeviceExtension->Int10Interface.Int10CallBios(
            VBEDeviceExtension->Int10Interface.Context,
            &BiosRegisters);

         VideoPortDebugPrint(Error, "KESHOOS VBEMP: Int10 VBE_GET_CONTROLLER result = 0x%lx\n", BiosRegisters.Eax);

         if (VBE_GETRETURNCODE(BiosRegisters.Eax) == VBE_SUCCESS)
         {
            VBEDeviceExtension->Int10Interface.Int10ReadMemory(
               VBEDeviceExtension->Int10Interface.Context,
               VBEDeviceExtension->TrampolineMemorySegment,
               VBEDeviceExtension->TrampolineMemoryOffset,
               &VBEDeviceExtension->VbeInfo,
               sizeof(VBEDeviceExtension->VbeInfo));

            /* Verify the VBE signature. */
            if (VideoPortCompareMemory(VBEDeviceExtension->VbeInfo.Signature, "VESA", 4) == 4)
            {
               VideoPortDebugPrint(Error, "KESHOOS VBEMP: VBE BIOS Present (%d.%d, %8ld Kb)\n",
                  VBEDeviceExtension->VbeInfo.Version / 0x100,
                  VBEDeviceExtension->VbeInfo.Version & 0xFF,
                  VBEDeviceExtension->VbeInfo.TotalMemory * 64);

#ifdef VBE12_SUPPORT
               if (VBEDeviceExtension->VbeInfo.Version >= 0x102)
#else
               if (VBEDeviceExtension->VbeInfo.Version >= 0x200)
#endif
               {
                  VbeFound = TRUE;
               }
            }
         }
      }
   }

   if (!VbeFound)
   {
      VideoPortDebugPrint(Error, "KESHOOS VBEMP: VbeFound == FALSE! Trying fallback/GOP...\n");
      PHYSICAL_ADDRESS VramAddress;
      ULONG VramSize = 0;
      VBE_FRAMEBUF_DATA VideoConfigData;
      BOOLEAN FramebufferFound = FALSE;

      VideoPortZeroMemory(&VBEDeviceExtension->Int10Interface, sizeof(VBEDeviceExtension->Int10Interface));
      VideoPortZeroMemory(&VramAddress, sizeof(VramAddress));
      VideoPortZeroMemory(&VideoConfigData, sizeof(VideoConfigData));

      if (VBEFindBootDisplay(&VramAddress, &VramSize, &VideoConfigData) && VramAddress.QuadPart != 0)
      {
         FramebufferFound = TRUE;
      }
      else
      {
         /* Query PCI Configuration Space for Display Controller BAR0 */
         typedef struct _VBE_PCI_CONFIG {
            USHORT VendorID;
            USHORT DeviceID;
            USHORT Command;
            USHORT Status;
            UCHAR RevisionID;
            UCHAR ProgIf;
            UCHAR SubClass;
            UCHAR BaseClass;
            UCHAR CacheLineSize;
            UCHAR LatencyTimer;
            UCHAR HeaderType;
            UCHAR BIST;
            ULONG BaseAddresses[6];
         } VBE_PCI_CONFIG;

         VBE_PCI_CONFIG PciData;
         ULONG BytesRead;

         VideoPortZeroMemory(&PciData, sizeof(PciData));
         BytesRead = VideoPortGetBusData(HwDeviceExtension,
                                         PCIConfiguration,
                                         0,
                                         &PciData,
                                         0,
                                         sizeof(PciData));
         if (BytesRead >= 0x20)
         {
            ULONG Bar0 = PciData.BaseAddresses[0] & 0xFFFFFFF0;
            if (Bar0 != 0)
            {
               VramAddress.QuadPart = Bar0;
               VramSize = 16 * 1024 * 1024;
               VideoConfigData.Version = 1;
               VideoConfigData.Revision = 3;
               VideoConfigData.ScreenWidth = 1024;
               VideoConfigData.ScreenHeight = 768;
               VideoConfigData.PixelsPerScanLine = 1024;
               VideoConfigData.BitsPerPixel = 32;
               VideoConfigData.FrameBufferOffset = 0;
               FramebufferFound = TRUE;
            }
         }

         /* Ultimate fallback for standard PC/VirtualBox/QEMU video RAM */
         if (!FramebufferFound)
         {
            VramAddress.QuadPart = 0xE0000000;
            VramSize = 16 * 1024 * 1024;
            VideoConfigData.Version = 1;
            VideoConfigData.Revision = 3;
            VideoConfigData.ScreenWidth = 1024;
            VideoConfigData.ScreenHeight = 768;
            VideoConfigData.PixelsPerScanLine = 1024;
            VideoConfigData.BitsPerPixel = 32;
            VideoConfigData.FrameBufferOffset = 0;
            FramebufferFound = TRUE;
         }
      }

      if (FramebufferFound)
      {
         VideoPortDebugPrint(Info, "VBEMP: Initializing Framebuffer mode: %lux%lu@%lu at 0x%I64X (Size %lu)\n",
                             VideoConfigData.ScreenWidth, VideoConfigData.ScreenHeight,
                             VideoConfigData.BitsPerPixel, VramAddress.QuadPart, VramSize);

         return VBEInitializeUefiGop(HwDeviceExtension, &VramAddress, VramSize, &VideoConfigData);
      }

      VideoPortDebugPrint(Error, "VBEMP: Neither VBE BIOS nor UEFI Framebuffer found.\n");
      return FALSE;
   }

   /*
    * Build a mode list here that can be later used by
    * IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES and IOCTL_VIDEO_QUERY_AVAIL_MODES
    * calls.
    */

   /*
    * Get the number of supported video modes.
    *
    * No need to be map the memory. It's either in the video BIOS memory or
    * in our trampoline memory. In either case the memory is already mapped.
    */

   for (ModeCount = 0; ; ModeCount++)
   {
      /* Read the VBE mode number. */
      VBEDeviceExtension->Int10Interface.Int10ReadMemory(
         VBEDeviceExtension->Int10Interface.Context,
         HIWORD(VBEDeviceExtension->VbeInfo.VideoModePtr),
         LOWORD(VBEDeviceExtension->VbeInfo.VideoModePtr) + (ModeCount << 1),
         &ModeTemp,
         sizeof(ModeTemp));

      /* End of list? */
      if (ModeTemp == 0xFFFF || ModeTemp == 0)
         break;
   }

   /*
    * Allocate space for video modes information.
    */

   VBEDeviceExtension->ModeInfo =
      VideoPortAllocatePool(HwDeviceExtension, VpPagedPool, ModeCount * sizeof(VBE_MODEINFO), TAG_VBE);
   VBEDeviceExtension->ModeNumbers =
      VideoPortAllocatePool(HwDeviceExtension, VpPagedPool, ModeCount * sizeof(USHORT), TAG_VBE);

   /*
    * Get the actual mode infos.
    */

   for (CurrentMode = 0, SuitableModeCount = 0;
        CurrentMode < ModeCount;
        CurrentMode++)
   {
      /* Read the VBE mode number. */
      VBEDeviceExtension->Int10Interface.Int10ReadMemory(
         VBEDeviceExtension->Int10Interface.Context,
         HIWORD(VBEDeviceExtension->VbeInfo.VideoModePtr),
         LOWORD(VBEDeviceExtension->VbeInfo.VideoModePtr) + (CurrentMode << 1),
         &ModeTemp,
         sizeof(ModeTemp));

      /* Call VBE BIOS to read the mode info. */
      VideoPortZeroMemory(&BiosRegisters, sizeof(BiosRegisters));
      BiosRegisters.Eax = VBE_GET_MODE_INFORMATION;
      BiosRegisters.Ecx = ModeTemp;
      BiosRegisters.Edi = VBEDeviceExtension->TrampolineMemoryOffset + 0x200;
      BiosRegisters.SegEs = VBEDeviceExtension->TrampolineMemorySegment;
      VBEDeviceExtension->Int10Interface.Int10CallBios(
         VBEDeviceExtension->Int10Interface.Context,
         &BiosRegisters);

      /* Read the VBE mode info. */
      VBEDeviceExtension->Int10Interface.Int10ReadMemory(
         VBEDeviceExtension->Int10Interface.Context,
         VBEDeviceExtension->TrampolineMemorySegment,
         VBEDeviceExtension->TrampolineMemoryOffset + 0x200,
         VBEDeviceExtension->ModeInfo + SuitableModeCount,
         sizeof(VBE_MODEINFO));

      VbeModeInfo = VBEDeviceExtension->ModeInfo + SuitableModeCount;

      /* Is this mode acceptable? */
      if (VBE_GETRETURNCODE(BiosRegisters.Eax) == VBE_SUCCESS &&
          VbeModeInfo->XResolution >= 640 &&
          VbeModeInfo->YResolution >= 480 &&
          (VbeModeInfo->MemoryModel == VBE_MEMORYMODEL_PACKEDPIXEL ||
           VbeModeInfo->MemoryModel == VBE_MEMORYMODEL_DIRECTCOLOR) &&
          VbeModeInfo->PhysBasePtr != 0)
      {
         if (VbeModeInfo->ModeAttributes & VBE_MODEATTR_LINEAR)
         {
            /* Bit 15 14 13 12 | 11 10 9 8 | 7 6 5 4 | 3 2 1 0 */
             // if (ModeTemp & 0x4000)
             //{
                VBEDeviceExtension->ModeNumbers[SuitableModeCount] = ModeTemp | 0x4000;
                SuitableModeCount++;
             //}
         }
#ifdef VBE12_SUPPORT
         else
         {
            VBEDeviceExtension->ModeNumbers[SuitableModeCount] = ModeTemp;
            SuitableModeCount++;
         }
#endif
      }
   }


   if (SuitableModeCount == 0)
   {

      VideoPortDebugPrint(Warn, "VBEMP: No video modes supported\n");
      return FALSE;
   }

   VBEDeviceExtension->ModeCount = SuitableModeCount;

   /*
    * Sort the video mode list according to resolution and bits per pixel.
    */

   VBESortModes(VBEDeviceExtension);

   /*
    * Print the supported video modes.
    */

   for (CurrentMode = 0;
        CurrentMode < SuitableModeCount;
        CurrentMode++)
   {
      VideoPortDebugPrint(Trace, "%dx%dx%d\n",
         VBEDeviceExtension->ModeInfo[CurrentMode].XResolution,
         VBEDeviceExtension->ModeInfo[CurrentMode].YResolution,
         VBEDeviceExtension->ModeInfo[CurrentMode].BitsPerPixel);
   }

   /*
    * Enumerate our children.
    */
   VideoPortEnumerateChildren(HwDeviceExtension, NULL);

   return TRUE;
}

/*
 * VBEStartIO
 *
 * Processes the specified Video Request Packet.
 */

BOOLEAN NTAPI
VBEStartIO(
   PVOID HwDeviceExtension,
   PVIDEO_REQUEST_PACKET RequestPacket)
{
   BOOLEAN Result;

   RequestPacket->StatusBlock->Status = ERROR_INVALID_FUNCTION;

   switch (RequestPacket->IoControlCode)
   {
      case IOCTL_VIDEO_SET_CURRENT_MODE:
         if (RequestPacket->InputBufferLength < sizeof(VIDEO_MODE))
         {
            RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
            return TRUE;
         }
         Result = VBESetCurrentMode(
            (PVBE_DEVICE_EXTENSION)HwDeviceExtension,
            (PVIDEO_MODE)RequestPacket->InputBuffer,
            RequestPacket->StatusBlock);
         break;

      case IOCTL_VIDEO_RESET_DEVICE:
         Result = VBEResetDevice(
            (PVBE_DEVICE_EXTENSION)HwDeviceExtension,
            RequestPacket->StatusBlock);
         break;

      case IOCTL_VIDEO_MAP_VIDEO_MEMORY:
         if (RequestPacket->OutputBufferLength < sizeof(VIDEO_MEMORY_INFORMATION) ||
             RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY))
         {
            RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
            return TRUE;
         }
         Result = VBEMapVideoMemory(
            (PVBE_DEVICE_EXTENSION)HwDeviceExtension,
            (PVIDEO_MEMORY)RequestPacket->InputBuffer,
            (PVIDEO_MEMORY_INFORMATION)RequestPacket->OutputBuffer,
            RequestPacket->StatusBlock);
         break;

      case IOCTL_VIDEO_UNMAP_VIDEO_MEMORY:
         if (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY))
         {
            RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
            return TRUE;
         }
         Result = VBEUnmapVideoMemory(
            (PVBE_DEVICE_EXTENSION)HwDeviceExtension,
            (PVIDEO_MEMORY)RequestPacket->InputBuffer,
            RequestPacket->StatusBlock);
         break;

      case IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES:
         if (RequestPacket->OutputBufferLength < sizeof(VIDEO_NUM_MODES))
         {
            RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
            return TRUE;
         }
         Result = VBEQueryNumAvailModes(
            (PVBE_DEVICE_EXTENSION)HwDeviceExtension,
            (PVIDEO_NUM_MODES)RequestPacket->OutputBuffer,
            RequestPacket->StatusBlock);
         break;

      case IOCTL_VIDEO_QUERY_AVAIL_MODES:
         if (RequestPacket->OutputBufferLength <
             ((PVBE_DEVICE_EXTENSION)HwDeviceExtension)->ModeCount * sizeof(VIDEO_MODE_INFORMATION))
         {
            RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
            return TRUE;
         }
         Result = VBEQueryAvailModes(
            (PVBE_DEVICE_EXTENSION)HwDeviceExtension,
            (PVIDEO_MODE_INFORMATION)RequestPacket->OutputBuffer,
            RequestPacket->StatusBlock);
         break;

      case IOCTL_VIDEO_SET_COLOR_REGISTERS:
         if (RequestPacket->InputBufferLength < sizeof(VIDEO_CLUT) ||
             RequestPacket->InputBufferLength <
             (((PVIDEO_CLUT)RequestPacket->InputBuffer)->NumEntries * sizeof(ULONG)) +
             FIELD_OFFSET(VIDEO_CLUT, LookupTable))
         {
            RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
            return TRUE;
         }
         Result = VBESetColorRegisters(
            (PVBE_DEVICE_EXTENSION)HwDeviceExtension,
            (PVIDEO_CLUT)RequestPacket->InputBuffer,
            RequestPacket->StatusBlock);
         break;

      case IOCTL_VIDEO_QUERY_CURRENT_MODE:
         if (RequestPacket->OutputBufferLength < sizeof(VIDEO_MODE_INFORMATION))
         {
            RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
            return TRUE;
         }
         Result = VBEQueryCurrentMode(
            (PVBE_DEVICE_EXTENSION)HwDeviceExtension,
            (PVIDEO_MODE_INFORMATION)RequestPacket->OutputBuffer,
            RequestPacket->StatusBlock);
         break;

      default:
         RequestPacket->StatusBlock->Status = ERROR_INVALID_FUNCTION;
         return FALSE;
   }

   if (Result)
      RequestPacket->StatusBlock->Status = NO_ERROR;

   return TRUE;
}

/*
 * VBEResetHw
 *
 * This function is called to reset the hardware to a known state.
 */

BOOLEAN NTAPI
VBEResetHw(
   PVOID DeviceExtension,
   ULONG Columns,
   ULONG Rows)
{
   /* Return FALSE to let HAL reset the display with INT10 */
   return FALSE;
}

/*
 * VBEGetPowerState
 *
 * Queries whether the device can support the requested power state.
 */

VP_STATUS NTAPI
VBEGetPowerState(
   PVOID HwDeviceExtension,
   ULONG HwId,
   PVIDEO_POWER_MANAGEMENT VideoPowerControl)
{
   INT10_BIOS_ARGUMENTS BiosRegisters;
   PVBE_DEVICE_EXTENSION VBEDeviceExtension =
     (PVBE_DEVICE_EXTENSION)HwDeviceExtension;

   if (HwId != DISPLAY_ADAPTER_HW_ID ||
       VideoPowerControl->Length < sizeof(VIDEO_POWER_MANAGEMENT))
      return ERROR_INVALID_FUNCTION;

   if (VBEDeviceExtension->Int10Interface.Int10CallBios == NULL)
   {
      VideoPowerControl->PowerState = VideoPowerOn;
      return NO_ERROR;
   }

   /*
    * Get general power support information.
    */

   VideoPortZeroMemory(&BiosRegisters, sizeof(BiosRegisters));
   BiosRegisters.Eax = VBE_POWER_MANAGEMENT_EXTENSIONS;
   BiosRegisters.Ebx = 0;
   BiosRegisters.Edi = 0;
   BiosRegisters.SegEs = 0;
   VBEDeviceExtension->Int10Interface.Int10CallBios(
      VBEDeviceExtension->Int10Interface.Context,
      &BiosRegisters);

   if ( VBE_GETRETURNCODE(BiosRegisters.Eax) == VBE_NOT_SUPPORTED)
      return ERROR_DEV_NOT_EXIST;
   if (VBE_GETRETURNCODE(BiosRegisters.Eax) != VBE_SUCCESS)
      return ERROR_INVALID_FUNCTION;

   /*
    * Get current power state.
    */

   VideoPortZeroMemory(&BiosRegisters, sizeof(BiosRegisters));
   BiosRegisters.Eax = VBE_POWER_MANAGEMENT_EXTENSIONS;
   BiosRegisters.Ebx = 0x2;
   BiosRegisters.Edi = 0;
   BiosRegisters.SegEs = 0;
   VBEDeviceExtension->Int10Interface.Int10CallBios(
      VBEDeviceExtension->Int10Interface.Context,
      &BiosRegisters);

   if (VBE_GETRETURNCODE(BiosRegisters.Eax) == VBE_SUCCESS)
   {
      VideoPowerControl->DPMSVersion = BiosRegisters.Ebx & 0xFF;
      switch (BiosRegisters.Ebx >> 8)
      {
         case 0: VideoPowerControl->PowerState = VideoPowerOn; break;
         case 1: VideoPowerControl->PowerState = VideoPowerStandBy; break;
         case 2: VideoPowerControl->PowerState = VideoPowerSuspend; break;
         case 4: VideoPowerControl->PowerState = VideoPowerOff; break;
         case 5: VideoPowerControl->PowerState = VideoPowerOn; break;
         default: VideoPowerControl->PowerState = VideoPowerUnspecified;
      }

      return NO_ERROR;
   }

   return ERROR_DEV_NOT_EXIST;
}

/*
 * VBESetPowerState
 *
 * Sets the power state of the specified device
 */

VP_STATUS NTAPI
VBESetPowerState(
   _In_ PVOID HwDeviceExtension,
   _In_ ULONG HwId,
   _In_ PVIDEO_POWER_MANAGEMENT VideoPowerControl)
{
   INT10_BIOS_ARGUMENTS BiosRegisters;
   PVBE_DEVICE_EXTENSION VBEDeviceExtension =
     (PVBE_DEVICE_EXTENSION)HwDeviceExtension;

   if (HwId != DISPLAY_ADAPTER_HW_ID ||
       VideoPowerControl->Length < sizeof(VIDEO_POWER_MANAGEMENT) ||
       VideoPowerControl->PowerState < VideoPowerOn ||
       VideoPowerControl->PowerState > VideoPowerHibernate)
      return ERROR_INVALID_FUNCTION;

   if (VideoPowerControl->PowerState == VideoPowerHibernate ||
       VBEDeviceExtension->Int10Interface.Int10CallBios == NULL)
      return NO_ERROR;

   /*
    * Set current power state.
    */

   VideoPortZeroMemory(&BiosRegisters, sizeof(BiosRegisters));
   BiosRegisters.Eax = VBE_POWER_MANAGEMENT_EXTENSIONS;
   BiosRegisters.Ebx = 1;
   BiosRegisters.Edi = 0;
   BiosRegisters.SegEs = 0;
   switch (VideoPowerControl->PowerState)
   {
      case VideoPowerStandBy: BiosRegisters.Ebx |= 0x100; break;
      case VideoPowerSuspend: BiosRegisters.Ebx |= 0x200; break;
      case VideoPowerOff: BiosRegisters.Ebx |= 0x400; break;
   }

   VBEDeviceExtension->Int10Interface.Int10CallBios(
      VBEDeviceExtension->Int10Interface.Context,
      &BiosRegisters);

   if (VBE_GETRETURNCODE(BiosRegisters.Eax) == VBE_NOT_SUPPORTED)
      return ERROR_DEV_NOT_EXIST;
   if (VBE_GETRETURNCODE(BiosRegisters.Eax) != VBE_SUCCESS)
      return ERROR_INVALID_FUNCTION;

   return NO_ERROR;
}

/*
 * VBESetCurrentMode
 *
 * Sets the adapter to the specified operating mode.
 */

BOOLEAN FASTCALL
VBESetCurrentMode(
   PVBE_DEVICE_EXTENSION DeviceExtension,
   PVIDEO_MODE RequestedMode,
   PSTATUS_BLOCK StatusBlock)
{
   INT10_BIOS_ARGUMENTS BiosRegisters;

   if (RequestedMode->RequestedMode >= DeviceExtension->ModeCount)
   {
      return ERROR_INVALID_PARAMETER;
   }

   if (DeviceExtension->Int10Interface.Int10CallBios == NULL)
   {
      DeviceExtension->CurrentMode = RequestedMode->RequestedMode;
      VideoPortDebugPrint(Error, "KESHOOS VBEMP: VBESetCurrentMode (GOP/NoInt10) mode=%lu\n", RequestedMode->RequestedMode);
      return TRUE;
   }

   VideoPortDebugPrint(Error, "KESHOOS VBEMP: VBESetCurrentMode mode=%lu (VBE Mode 0x%x)...\n",
                       RequestedMode->RequestedMode, DeviceExtension->ModeNumbers[RequestedMode->RequestedMode]);

   VideoPortZeroMemory(&BiosRegisters, sizeof(BiosRegisters));
   BiosRegisters.Eax = VBE_SET_VBE_MODE;
   BiosRegisters.Ebx = DeviceExtension->ModeNumbers[RequestedMode->RequestedMode];
   DeviceExtension->Int10Interface.Int10CallBios(
      DeviceExtension->Int10Interface.Context,
      &BiosRegisters);

   if (VBE_GETRETURNCODE(BiosRegisters.Eax) == VBE_SUCCESS)
   {
      DeviceExtension->CurrentMode = RequestedMode->RequestedMode;
      VideoPortDebugPrint(Error, "KESHOOS VBEMP: VBESetCurrentMode SUCCESS (Eax=0x%lx)\n", BiosRegisters.Eax);
   }
   else
   {
      VideoPortDebugPrint(Error, "KESHOOS VBEMP: VBESetCurrentMode FAILED (Eax=0x%lx)\n", BiosRegisters.Eax);
      /* Keep previous CurrentMode so subsequent queries/unmaps don't read out of bounds */
   }

   return VBE_GETRETURNCODE(BiosRegisters.Eax) == VBE_SUCCESS;
}

/*
 * VBEResetDevice
 *
 * Resets the video hardware to the default mode, to which it was initialized
 * at system boot.
 */

BOOLEAN FASTCALL
VBEResetDevice(
   PVBE_DEVICE_EXTENSION DeviceExtension,
   PSTATUS_BLOCK StatusBlock)
{
   INT10_BIOS_ARGUMENTS BiosRegisters;

   if (DeviceExtension->Int10Interface.Int10CallBios == NULL)
      return TRUE;

   VideoPortZeroMemory(&BiosRegisters, sizeof(BiosRegisters));
   BiosRegisters.Eax = VBE_SET_VBE_MODE;
   BiosRegisters.Ebx = 0x3;
   DeviceExtension->Int10Interface.Int10CallBios(
      DeviceExtension->Int10Interface.Context,
      &BiosRegisters);

   return VBE_GETRETURNCODE(BiosRegisters.Eax) == VBE_SUCCESS;
}

/*
 * VBEMapVideoMemory
 *
 * Maps the video hardware frame buffer and video RAM into the virtual address
 * space of the requestor.
 */

BOOLEAN FASTCALL
VBEMapVideoMemory(
   PVBE_DEVICE_EXTENSION DeviceExtension,
   PVIDEO_MEMORY RequestedAddress,
   PVIDEO_MEMORY_INFORMATION MapInformation,
   PSTATUS_BLOCK StatusBlock)
{
   PHYSICAL_ADDRESS FrameBuffer;
   ULONG inIoSpace = VIDEO_MEMORY_SPACE_MEMORY | VIDEO_MEMORY_SPACE_P6CACHE;

   StatusBlock->Information = sizeof(VIDEO_MEMORY_INFORMATION);

   if (DeviceExtension->ModeInfo[DeviceExtension->CurrentMode].ModeAttributes &
       VBE_MODEATTR_LINEAR)
   {
      FrameBuffer.QuadPart =
         DeviceExtension->ModeInfo[DeviceExtension->CurrentMode].PhysBasePtr;
      MapInformation->VideoRamBase = RequestedAddress->RequestedVirtualAddress;
      if (DeviceExtension->VbeInfo.Version < 0x300)
      {
         MapInformation->VideoRamLength =
            DeviceExtension->ModeInfo[DeviceExtension->CurrentMode].BytesPerScanLine *
            DeviceExtension->ModeInfo[DeviceExtension->CurrentMode].YResolution;
      }
      else
      {
         MapInformation->VideoRamLength =
            DeviceExtension->ModeInfo[DeviceExtension->CurrentMode].LinBytesPerScanLine *
            DeviceExtension->ModeInfo[DeviceExtension->CurrentMode].YResolution;
      }
      VideoPortDebugPrint(Error, "KESHOOS VBEMP: VBEMapVideoMemory PhysBase=0x%I64X Len=%lu Mode=%lu (%lux%lu@%lu)\n",
                          FrameBuffer.QuadPart, MapInformation->VideoRamLength, DeviceExtension->CurrentMode,
                          DeviceExtension->ModeInfo[DeviceExtension->CurrentMode].XResolution,
                          DeviceExtension->ModeInfo[DeviceExtension->CurrentMode].YResolution,
                          DeviceExtension->ModeInfo[DeviceExtension->CurrentMode].BitsPerPixel);
   }
#ifdef VBE12_SUPPORT
   else
   {
      FrameBuffer.QuadPart = 0xA0000;
      MapInformation->VideoRamBase = RequestedAddress->RequestedVirtualAddress;
      MapInformation->VideoRamLength = 0x10000;
   }
#endif

   VP_STATUS vpStatus = VideoPortMapMemory(DeviceExtension, FrameBuffer,
      &MapInformation->VideoRamLength, &inIoSpace,
      &MapInformation->VideoRamBase);

   VideoPortDebugPrint(Error, "KESHOOS VBEMP: VideoPortMapMemory returned status=0x%lx VideoRamBase=%p Length=%lu\n",
                       vpStatus, MapInformation->VideoRamBase, MapInformation->VideoRamLength);

   MapInformation->FrameBufferBase = MapInformation->VideoRamBase;
   MapInformation->FrameBufferLength = MapInformation->VideoRamLength;

   return (vpStatus == NO_ERROR);
}

/*
 * VBEUnmapVideoMemory
 *
 * Releases a mapping between the virtual address space and the adapter's
 * frame buffer and video RAM.
 */

BOOLEAN FASTCALL
VBEUnmapVideoMemory(
   PVBE_DEVICE_EXTENSION DeviceExtension,
   PVIDEO_MEMORY VideoMemory,
   PSTATUS_BLOCK StatusBlock)
{
   VideoPortUnmapMemory(DeviceExtension, VideoMemory->RequestedVirtualAddress,
      NULL);
   return TRUE;
}

/*
 * VBEQueryNumAvailModes
 *
 * Returns the number of video modes supported by the adapter and the size
 * in bytes of the video mode information, which can be used to allocate a
 * buffer for an IOCTL_VIDEO_QUERY_AVAIL_MODES request.
 */

BOOLEAN FASTCALL
VBEQueryNumAvailModes(
   PVBE_DEVICE_EXTENSION DeviceExtension,
   PVIDEO_NUM_MODES Modes,
   PSTATUS_BLOCK StatusBlock)
{
   Modes->NumModes = DeviceExtension->ModeCount;
   Modes->ModeInformationLength = sizeof(VIDEO_MODE_INFORMATION);
   StatusBlock->Information = sizeof(VIDEO_NUM_MODES);
   return TRUE;
}

/*
 * VBEQueryMode
 *
 * Returns information about one particular video mode.
 */

VOID FASTCALL
VBEQueryMode(
   PVBE_DEVICE_EXTENSION DeviceExtension,
   PVIDEO_MODE_INFORMATION VideoMode,
   ULONG VideoModeId)
{
   PVBE_MODEINFO VBEMode = &DeviceExtension->ModeInfo[VideoModeId];
   ULONG dpi;

   VideoMode->Length = sizeof(VIDEO_MODE_INFORMATION);
   VideoMode->ModeIndex = VideoModeId;
   VideoMode->VisScreenWidth = VBEMode->XResolution;
   VideoMode->VisScreenHeight = VBEMode->YResolution;
   if (DeviceExtension->VbeInfo.Version < 0x300)
      VideoMode->ScreenStride = VBEMode->BytesPerScanLine;
   else
      VideoMode->ScreenStride = VBEMode->LinBytesPerScanLine;
   VideoMode->NumberOfPlanes = VBEMode->NumberOfPlanes;
   VideoMode->BitsPerPlane = VBEMode->BitsPerPixel / VBEMode->NumberOfPlanes;
   VideoMode->Frequency = 1;

   /* Assume 96DPI and 25.4 millimeters per inch, round to nearest */
   dpi = 96;
   VideoMode->XMillimeter = ((ULONGLONG)VBEMode->XResolution * 254 + (dpi * 5)) / (dpi * 10);
   VideoMode->YMillimeter = ((ULONGLONG)VBEMode->YResolution * 254 + (dpi * 5)) / (dpi * 10);

   if (VBEMode->BitsPerPixel > 8)
   {
      /*
       * Always report 16bpp modes and not 15bpp mode...
       */
      if (VBEMode->BitsPerPixel == 15 && VBEMode->NumberOfPlanes == 1)
      {
         VideoMode->BitsPerPlane = 16;
      }

      if (DeviceExtension->VbeInfo.Version < 0x300)
      {
         VideoMode->NumberRedBits = VBEMode->RedMaskSize;
         VideoMode->NumberGreenBits = VBEMode->GreenMaskSize;
         VideoMode->NumberBlueBits = VBEMode->BlueMaskSize;
         VideoMode->RedMask = ((1 << VBEMode->RedMaskSize) - 1) << VBEMode->RedFieldPosition;
         VideoMode->GreenMask = ((1 << VBEMode->GreenMaskSize) - 1) << VBEMode->GreenFieldPosition;
         VideoMode->BlueMask = ((1 << VBEMode->BlueMaskSize) - 1) << VBEMode->BlueFieldPosition;
      }
      else
      {
         VideoMode->NumberRedBits = VBEMode->LinRedMaskSize;
         VideoMode->NumberGreenBits = VBEMode->LinGreenMaskSize;
         VideoMode->NumberBlueBits = VBEMode->LinBlueMaskSize;
         VideoMode->RedMask = ((1 << VBEMode->LinRedMaskSize) - 1) << VBEMode->LinRedFieldPosition;
         VideoMode->GreenMask = ((1 << VBEMode->LinGreenMaskSize) - 1) << VBEMode->LinGreenFieldPosition;
         VideoMode->BlueMask = ((1 << VBEMode->LinBlueMaskSize) - 1) << VBEMode->LinBlueFieldPosition;
      }
   }
   else
   {
      VideoMode->NumberRedBits =
      VideoMode->NumberGreenBits =
      VideoMode->NumberBlueBits = 6;
      VideoMode->RedMask =
      VideoMode->GreenMask =
      VideoMode->BlueMask = 0;
   }
   VideoMode->VideoMemoryBitmapWidth = VBEMode->XResolution;
   VideoMode->VideoMemoryBitmapHeight = VBEMode->YResolution;
   VideoMode->AttributeFlags = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR |
      VIDEO_MODE_NO_OFF_SCREEN;
   if (VideoMode->BitsPerPlane <= 8)
      VideoMode->AttributeFlags |= VIDEO_MODE_PALETTE_DRIVEN;
   VideoMode->DriverSpecificAttributeFlags = 0;
}

/*
 * VBEQueryAvailModes
 *
 * Returns information about each video mode supported by the adapter.
 */

BOOLEAN FASTCALL
VBEQueryAvailModes(
   PVBE_DEVICE_EXTENSION DeviceExtension,
   PVIDEO_MODE_INFORMATION ReturnedModes,
   PSTATUS_BLOCK StatusBlock)
{
   ULONG CurrentModeId;
   PVIDEO_MODE_INFORMATION CurrentMode;
   PVBE_MODEINFO CurrentVBEMode;

   for (CurrentModeId = 0, CurrentMode = ReturnedModes,
        CurrentVBEMode = DeviceExtension->ModeInfo;
        CurrentModeId < DeviceExtension->ModeCount;
        CurrentModeId++, CurrentMode++, CurrentVBEMode++)
   {
      VBEQueryMode(DeviceExtension, CurrentMode, CurrentModeId);
   }

   StatusBlock->Information =
      sizeof(VIDEO_MODE_INFORMATION) * DeviceExtension->ModeCount;

   return TRUE;
}

/*
 * VBEQueryCurrentMode
 *
 * Returns information about current video mode.
 */

BOOLEAN FASTCALL
VBEQueryCurrentMode(
   PVBE_DEVICE_EXTENSION DeviceExtension,
   PVIDEO_MODE_INFORMATION VideoModeInfo,
   PSTATUS_BLOCK StatusBlock)
{
   StatusBlock->Information = sizeof(VIDEO_MODE_INFORMATION);

   VBEQueryMode(
      DeviceExtension,
      VideoModeInfo,
      DeviceExtension->CurrentMode);

   return TRUE;
}

/*
 * VBESetColorRegisters
 *
 * Sets the adapter's color registers to the specified RGB values. There
 * are code paths in this function, one generic and one for VGA compatible
 * controllers. The latter is needed for Bochs, where the generic one isn't
 * yet implemented.
 */

BOOLEAN FASTCALL
VBESetColorRegisters(
   PVBE_DEVICE_EXTENSION DeviceExtension,
   PVIDEO_CLUT ColorLookUpTable,
   PSTATUS_BLOCK StatusBlock)
{
   INT10_BIOS_ARGUMENTS BiosRegisters;
   ULONG Entry;
   PULONG OutputEntry;
   ULONG OutputBuffer[256];

   if (ColorLookUpTable->NumEntries + ColorLookUpTable->FirstEntry > 256)
      return FALSE;

   if (DeviceExtension->Int10Interface.Int10CallBios == NULL)
      return TRUE;

   /*
    * For VGA compatible adapters program the color registers directly.
    */

   if (!(DeviceExtension->VbeInfo.Capabilities & 2))
   {
      for (Entry = ColorLookUpTable->FirstEntry;
           Entry < ColorLookUpTable->NumEntries + ColorLookUpTable->FirstEntry;
           Entry++)
      {
         VideoPortWritePortUchar((PUCHAR)0x03c8, Entry);
         VideoPortWritePortUchar((PUCHAR)0x03c9, ColorLookUpTable->LookupTable[Entry].RgbArray.Red);
         VideoPortWritePortUchar((PUCHAR)0x03c9, ColorLookUpTable->LookupTable[Entry].RgbArray.Green);
         VideoPortWritePortUchar((PUCHAR)0x03c9, ColorLookUpTable->LookupTable[Entry].RgbArray.Blue);
      }

      return TRUE;
   }
   else
   {
      /*
       * We can't just copy the values, because we need to swap the Red
       * and Blue values.
       */

      for (Entry = ColorLookUpTable->FirstEntry,
           OutputEntry = OutputBuffer;
           Entry < ColorLookUpTable->NumEntries + ColorLookUpTable->FirstEntry;
           Entry++, OutputEntry++)
      {
         *OutputEntry =
            (ColorLookUpTable->LookupTable[Entry].RgbArray.Red << 16) |
            (ColorLookUpTable->LookupTable[Entry].RgbArray.Green << 8) |
            (ColorLookUpTable->LookupTable[Entry].RgbArray.Blue);
      }

      DeviceExtension->Int10Interface.Int10WriteMemory(
         DeviceExtension->Int10Interface.Context,
         DeviceExtension->TrampolineMemorySegment,
         DeviceExtension->TrampolineMemoryOffset,
         OutputBuffer,
         (OutputEntry - OutputBuffer) * sizeof(ULONG));

      VideoPortZeroMemory(&BiosRegisters, sizeof(BiosRegisters));
      BiosRegisters.Eax = VBE_SET_GET_PALETTE_DATA;
      BiosRegisters.Ebx = 0;
      BiosRegisters.Ecx = ColorLookUpTable->NumEntries;
      BiosRegisters.Edx = ColorLookUpTable->FirstEntry;
      BiosRegisters.Edi = DeviceExtension->TrampolineMemoryOffset;
      BiosRegisters.SegEs = DeviceExtension->TrampolineMemorySegment;
      DeviceExtension->Int10Interface.Int10CallBios(
         DeviceExtension->Int10Interface.Context,
         &BiosRegisters);

      return VBE_GETRETURNCODE(BiosRegisters.Eax) == VBE_SUCCESS;
   }
}
