// SPDX-License-Identifier: MIT
#pragma once
#include "SonyUfsContract.h"
#include "SonyFixture.h"
STATIC SONY_UFS_CONTEXT *SonyContext;
STATIC UINT64 SonyUfsContextPa;
STATIC UINT32 SonyReg(UINTN Off){UINT32 V=*(volatile UINT32 *)(UINTN)(0x01da4000+Off);__asm__ volatile("dmb oshld":::"memory");return V;}
STATIC EFI_STATUS SonyUfsPrepare(VOID){
 UINT64 Midr;EFI_PHYSICAL_ADDRESS Pa=0,Area=0;EFI_STATUS Status;
 __asm__ volatile("mrs %0,midr_el1":"=r"(Midr));
 if(Midr!=0x51af8014){DEBUG((DEBUG_INFO,"S4D_UFS_CONTEXT_SKIP qemu=1\n"));return EFI_SUCCESS;}
 if(SonyReg(0x34)!=1 || (SonyReg(0x30)&3)!=3 || SonyReg(0x58) || SonyReg(0x78) || SonyReg(0x24))return EFI_NOT_READY;
 Status=gBS->AllocatePages(AllocateAnyPages,EfiLoaderData,1,&Pa);if(EFI_ERROR(Status))return Status;
 Status=gBS->AllocatePages(AllocateAnyPages,EfiLoaderData,16,&Area);if(EFI_ERROR(Status)){gBS->FreePages(Pa,1);return Status;}
 if(Pa<0xa0200000 || Pa+4096>0xa3fc0000 || Area<0xa0200000 || Area+65536>0xa3fc0000){gBS->FreePages(Area,16);gBS->FreePages(Pa,1);return EFI_COMPROMISED_DATA;}
 SonyContext=(SONY_UFS_CONTEXT *)(UINTN)Pa;ZeroMem(SonyContext,4096);ZeroMem((VOID *)(UINTN)Area,65536);
 SonyContext->Magic=SONY_UFS_MAGIC;SonyContext->Version=1;SonyContext->Size=4096;SonyContext->Hc=0x01da4000;
 SonyContext->Area=Area;SonyContext->AreaSize=65536;SonyContext->UcdOffset=1024;SonyContext->UcdSize=1152;SonyContext->Slots=32;
 SonyContext->OldLo=SonyReg(0x50);SonyContext->OldHi=SonyReg(0x54);SonyContext->OldRun=SonyReg(0x60);
 SonyContext->FileCount=1;SonyContext->ExtentCount=1;SonyContext->PartitionFirst=270601;SonyContext->PartitionBlocks=13296370;SonyContext->DiskBlocks=13566976;
 SonyContext->File[0].Bytes=65536;SonyContext->File[0].Count=1;CopyMem(SonyContext->File[0].Name,"fixture",8);SonyContext->File[0].ProofFnv=SONY_FIXTURE_FNV;
 SonyContext->Extent[0].Logical=0;SonyContext->Extent[0].Lba=SONY_FIXTURE_LBA;SonyContext->Extent[0].Blocks=16;
 SonyUfsContextPa=Pa;
 DEBUG((DEBUG_INFO,"S4D_UFS_CONTEXT pa=%lx dma=%lx old_list=%x:%x fixture_lba=%lu expected_fnv=%lx\n",Pa,Area,SonyContext->OldHi,SonyContext->OldLo,(UINT64)SONY_FIXTURE_LBA,(UINT64)SONY_FIXTURE_FNV));
 return EFI_SUCCESS;
}
STATIC EFI_STATUS SonyUfsFinish(VOID){
 UINT64 Ctr;UINTN A,Line;BOOLEAN Restored;EFI_STATUS Status;
 if(!SonyUfsContextPa)return EFI_SUCCESS;
 __asm__ volatile("mrs %0,ctr_el0":"=r"(Ctr));Line=4UL<<((Ctr>>16)&15);
 __asm__ volatile("dsb sy":::"memory");for(A=(UINTN)SonyContext;A<(UINTN)SonyContext+4096;A+=Line)__asm__ volatile("dc ivac,%0"::"r"(A):"memory");__asm__ volatile("dsb sy":::"memory");
 Restored=SonyReg(0x50)==SonyContext->OldLo && SonyReg(0x54)==SonyContext->OldHi && SonyReg(0x60)==SonyContext->OldRun && !SonyReg(0x58) && !SonyReg(0x78) && !SonyContext->ListIn;
 DEBUG((DEBUG_INFO,"S4D_UFS_RETURN restored=%u list_in=%u switched_in=%u switched_out=%u db=%x\n",Restored,SonyContext->ListIn,SonyContext->SwitchIn,SonyContext->SwitchOut,SonyReg(0x58)));
 if(!Restored)return EFI_DEVICE_ERROR;
 if(!SonyContext->SwitchIn || !SonyContext->SwitchOut)return EFI_NOT_READY;
 Status=gBS->FreePages(SonyContext->Area,16);if(EFI_ERROR(Status))return Status;
 Status=gBS->FreePages(SonyUfsContextPa,1);SonyContext=NULL;SonyUfsContextPa=0;return Status;
}
