// SPDX-License-Identifier: MIT
#include <PiPei.h>
#include <Library/ArmMmuLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>

EFI_STATUS EFIAPI MemoryPeim (EFI_PHYSICAL_ADDRESS UefiMemoryBase, UINT64 UefiMemorySize) {
  // Deliberately expose only this 62 MiB arena to DXE allocations in the first probe.
  // The SO-04J DTB and all other reserved regions stay outside the allocator.
  ARM_MEMORY_REGION_DESCRIPTOR Map[] = {
    {0xa0000000, 0xa0000000, 0x04000000, ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK},
    {0xffb00000, 0xffb00000, 0x00100000, ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK},
    {0xffc00000, 0xffc00000, 0x00100000, ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK},
    {0x010ac000, 0x010ac000, 0x00001000, ARM_MEMORY_REGION_ATTRIBUTE_DEVICE},
    {0,0,0,0}
  };
  VOID *Table;
  UINTN TableSize;
  RETURN_STATUS Status;
  UINT64 Cap = EFI_RESOURCE_ATTRIBUTE_PRESENT | EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
    EFI_RESOURCE_ATTRIBUTE_TESTED | EFI_RESOURCE_ATTRIBUTE_UNCACHEABLE |
    EFI_RESOURCE_ATTRIBUTE_WRITE_COMBINEABLE | EFI_RESOURCE_ATTRIBUTE_WRITE_THROUGH_CACHEABLE |
    EFI_RESOURCE_ATTRIBUTE_WRITE_BACK_CACHEABLE;
  DEBUG ((DEBUG_INFO, "SONY_MEMORY_BEGIN base=%lx size=%lx\n", UefiMemoryBase, UefiMemorySize));
  BuildResourceDescriptorHob (EFI_RESOURCE_SYSTEM_MEMORY, Cap, 0xa0000000, 0x04000000);
  BuildMemoryAllocationHob (0xa0000000, 0x00200000, EfiBootServicesData);
  BuildResourceDescriptorHob (EFI_RESOURCE_MEMORY_RESERVED, Cap, 0xffb00000, 0x00200000);
  BuildMemoryAllocationHob (0xffb00000, 0x00200000, EfiReservedMemoryType);
  Status = ArmConfigureMmu (Map, &Table, &TableSize);
  DEBUG ((DEBUG_INFO, "SONY_MMU_RETURN status=%r table=%p size=%lx\n", Status, Table, TableSize));
  return Status;
}
