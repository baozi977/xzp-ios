// SPDX-License-Identifier: MIT
#pragma once
#include "SonyUfsContract.h"
#include "SonyFixture.h"
#include "SonyFiles.h"
#include <Library/TimerLib.h>
STATIC SONY_UFS_CONTEXT *SonyContext;
STATIC UINT64 SonyUfsContextPa;
STATIC EFI_PHYSICAL_ADDRESS SonyPrime,SonyContextAllocation;
STATIC UINT32 SonyRealLo,SonyRealHi,SonyRealRun;
STATIC UINT32 SonyReg(UINTN Off){UINT32 V=*(volatile UINT32 *)(UINTN)(0x01da4000+Off);__asm__ volatile("dmb oshld":::"memory");return V;}
STATIC VOID SonyWrite(UINTN Off,UINT32 V){__asm__ volatile("dsb sy":::"memory");*(volatile UINT32 *)(UINTN)(0x01da4000+Off)=V;__asm__ volatile("dsb sy":::"memory");}
STATIC BOOLEAN SonyWait(UINTN Off,UINT32 Mask,UINT32 V,UINTN Us){UINTN I;for(I=0;I<Us;I+=10){if((SonyReg(Off)&Mask)==V)return TRUE;MicroSecondDelay(10);}return FALSE;}
STATIC VOID SonyCache(VOID *Buffer,UINTN Size,BOOLEAN After){
 UINT64 Ctr;UINTN A,Line;__asm__ volatile("mrs %0,ctr_el0":"=r"(Ctr));Line=4UL<<((Ctr>>16)&15);
 __asm__ volatile("dsb sy":::"memory");for(A=(UINTN)Buffer;A<(UINTN)Buffer+Size;A+=Line){
  if(After)__asm__ volatile("dc ivac,%0"::"r"(A):"memory");else __asm__ volatile("dc civac,%0"::"r"(A):"memory");
 }__asm__ volatile("dsb sy":::"memory");
}
STATIC BOOLEAN SonyRestoreFirmware(VOID){
 UINT32 Lo,Hi;
 if(!SonyWait(0x58,0xffffffff,0,1000000) || SonyReg(0x78))return FALSE;
 Lo=SonyReg(0x50);Hi=SonyReg(0x54);
 if(!((Lo==(UINT32)SonyPrime && Hi==(UINT32)(SonyPrime>>32)) || (Lo==SonyRealLo && Hi==SonyRealHi)))return FALSE;
 SonyWrite(0x60,0);if(!SonyWait(0x60,1,0,10000))return FALSE;
 SonyWrite(0x50,SonyRealLo);SonyWrite(0x54,SonyRealHi);SonyWrite(0x60,SonyRealRun);
 return SonyReg(0x50)==SonyRealLo && SonyReg(0x54)==SonyRealHi && SonyReg(0x60)==SonyRealRun && !SonyReg(0x58);
}
STATIC EFI_STATUS SonyPrimeRead(VOID){
 UINT8 *M=(UINT8 *)(UINTN)SonyPrime,*Q=M+4096,*Rsp=M+4608;
 UINT32 *D=(UINT32 *)M,*Prd=(UINT32 *)(M+5120),Lba=(UINT32)SONY_FIXTURE_LBA,Crc=0;
 ZeroMem(M,65536);Q[0]=1;Q[1]=0x40;Q[12]=0;Q[13]=0;Q[14]=0x80;Q[15]=0;
 Q[16]=0x28;Q[18]=Lba>>24;Q[19]=Lba>>16;Q[20]=Lba>>8;Q[21]=Lba;Q[24]=8;
 D[0]=0x15000000;D[2]=15;D[4]=(UINT32)(SonyPrime+4096);D[5]=(UINT32)((SonyPrime+4096)>>32);
 D[6]=(128U<<16)|128;D[7]=(256U<<16)|1;
 Prd[0]=(UINT32)(SonyPrime+16384);Prd[1]=(UINT32)((SonyPrime+16384)>>32);Prd[3]=32767;
 SonyCache(M,65536,FALSE);
 if(SonyReg(0x58) || SonyReg(0x78) || SonyReg(0x50)!=SonyRealLo || SonyReg(0x54)!=SonyRealHi)return EFI_NOT_READY;
 SonyWrite(0x60,0);if(!SonyWait(0x60,1,0,10000))return EFI_TIMEOUT;
 SonyWrite(0x50,(UINT32)SonyPrime);SonyWrite(0x54,(UINT32)(SonyPrime>>32));SonyWrite(0x60,1);
 if(!SonyWait(0x60,1,1,10000) || SonyReg(0x50)!=(UINT32)SonyPrime || SonyReg(0x54)!=(UINT32)(SonyPrime>>32) || SonyReg(0x58))return EFI_DEVICE_ERROR;
 // Ring only our exact READ(10), never the firmware's retained WRITE descriptor.
 if(D[0]!=0x15000000 || Q[0]!=1 || Q[1]!=0x40 || Q[2]!=0 || Q[16]!=0x28 || Q[24]!=8)return EFI_ACCESS_DENIED;
 SonyWrite(0x58,1);if(!SonyWait(0x58,1,0,250000))return EFI_TIMEOUT;
 SonyCache(M,65536,TRUE);
 if((D[2]&255) || Rsp[0]!=0x21 || Rsp[1] || Rsp[2] || Rsp[3] || Rsp[4] || Rsp[6] || Rsp[7] || Rsp[10] || Rsp[11] || Rsp[12] || Rsp[13] || Rsp[14] || Rsp[15])return EFI_DEVICE_ERROR;
 gBS->CalculateCrc32(M+16384,32768,&Crc);
 DEBUG((DEBUG_INFO,"S4J_UFS_PRIME_READ crc=%x expected=FFE535DF ocs=%x readonly=1\n",Crc,D[2]&255));
 if(Crc!=0xffe535df)return EFI_CRC_ERROR;
 return EFI_SUCCESS;
}
STATIC EFI_STATUS SonyUfsPrepare(VOID){
 UINT64 Midr;EFI_PHYSICAL_ADDRESS Pa=0,Area=0;EFI_STATUS Status;
 __asm__ volatile("mrs %0,midr_el1":"=r"(Midr));
 if(Midr!=0x51af8014){DEBUG((DEBUG_INFO,"S4J_UFS_CONTEXT_SKIP qemu=1\n"));return EFI_SUCCESS;}
 if(SonyReg(0x34)!=1 || (SonyReg(0x30)&3)!=3 || SonyReg(0x58) || SonyReg(0x78) || SonyReg(0x24) || SonyReg(0x60)!=1)return EFI_NOT_READY;
 Status=gBS->AllocatePages(AllocateAnyPages,EfiLoaderData,7,&SonyContextAllocation);if(EFI_ERROR(Status))return Status;
 Pa=(SonyContextAllocation+16383)&~16383ULL;
 Status=gBS->AllocatePages(AllocateAnyPages,EfiLoaderData,16,&Area);if(EFI_ERROR(Status)){gBS->FreePages(SonyContextAllocation,7);return Status;}
 Status=gBS->AllocatePages(AllocateAnyPages,EfiLoaderData,16,&SonyPrime);if(EFI_ERROR(Status)){gBS->FreePages(Area,16);gBS->FreePages(SonyContextAllocation,7);return Status;}
 if(SonyContextAllocation<0xa0200000 || SonyContextAllocation+7*4096>0xa3fc0000 || Pa+16384>SonyContextAllocation+7*4096 || Pa<0xa0200000 || Pa+16384>0xa3fc0000 || Area<0xa0200000 || Area+65536>0xa3fc0000 || SonyPrime<0xa0200000 || SonyPrime+65536>0xa3fc0000){gBS->FreePages(SonyPrime,16);gBS->FreePages(Area,16);gBS->FreePages(SonyContextAllocation,7);return EFI_COMPROMISED_DATA;}
 SonyContext=(SONY_UFS_CONTEXT *)(UINTN)Pa;ZeroMem(SonyContext,4096);ZeroMem((VOID *)(UINTN)Area,65536);
 SonyContext->Magic=SONY_UFS_MAGIC;SonyContext->Version=1;SonyContext->Size=4096;SonyContext->Hc=0x01da4000;
 SonyContext->Area=Area;SonyContext->AreaSize=65536;SonyContext->UcdOffset=1024;SonyContext->UcdSize=1152;SonyContext->Slots=32;
 SonyRealLo=SonyReg(0x50);SonyRealHi=SonyReg(0x54);SonyRealRun=SonyReg(0x60);
 SonyContext->OldLo=(UINT32)SonyPrime;SonyContext->OldHi=(UINT32)(SonyPrime>>32);SonyContext->OldRun=1;
 SonyContext->PartitionFirst=270601;SonyContext->PartitionBlocks=13296370;SonyContext->DiskBlocks=13566976;
 SonyFillFiles(SonyContext);
 SonyUfsContextPa=Pa;
 Status=SonyPrimeRead();
 if(EFI_ERROR(Status)){
  BOOLEAN Back=SonyRestoreFirmware();DEBUG((DEBUG_ERROR,"S4J_UFS_PRIME_FAILED status=%r restored=%u\n",Status,Back));
  if(Back){gBS->FreePages(SonyPrime,16);gBS->FreePages(Area,16);gBS->FreePages(SonyContextAllocation,7);SonyContext=NULL;SonyUfsContextPa=0;}
  return Status;
 }
 DEBUG((DEBUG_INFO,"S4J_UFS_CONTEXT pa=%lx dma=%lx old_list=%x:%x fixture_lba=%lu expected_fnv=%lx\n",Pa,Area,SonyContext->OldHi,SonyContext->OldLo,(UINT64)SONY_FIXTURE_LBA,(UINT64)SONY_FIXTURE_FNV));
 return EFI_SUCCESS;
}
STATIC EFI_STATUS SonyUfsFinish(VOID){
 BOOLEAN Restored,Used;EFI_STATUS Status;
 if(!SonyUfsContextPa)return EFI_SUCCESS;
 SonyCache(SonyContext,4096,TRUE);
 Restored=SonyReg(0x50)==SonyContext->OldLo && SonyReg(0x54)==SonyContext->OldHi && SonyReg(0x60)==SonyContext->OldRun && !SonyReg(0x58) && !SonyReg(0x78) && !SonyContext->ListIn;
 DEBUG((DEBUG_INFO,"S4J_UFS_RETURN restored=%u list_in=%u switched_in=%u switched_out=%u db=%x\n",Restored,SonyContext->ListIn,SonyContext->SwitchIn,SonyContext->SwitchOut,SonyReg(0x58)));
 if(!Restored)return EFI_DEVICE_ERROR;
 Used=SonyContext->SwitchIn && SonyContext->SwitchOut;
 Restored=SonyRestoreFirmware();DEBUG((DEBUG_INFO,"S4J_UFS_FIRMWARE_RESTORE restored=%u base=%x:%x db=%x\n",Restored,SonyReg(0x54),SonyReg(0x50),SonyReg(0x58)));
 if(!Restored)return EFI_DEVICE_ERROR;
 Status=gBS->FreePages(SonyPrime,16);if(EFI_ERROR(Status))return Status;
 Status=gBS->FreePages(SonyContext->Area,16);if(EFI_ERROR(Status))return Status;
 Status=gBS->FreePages(SonyContextAllocation,7);SonyContext=NULL;SonyUfsContextPa=0;if(EFI_ERROR(Status))return Status;return Used?EFI_SUCCESS:EFI_NOT_READY;
}
