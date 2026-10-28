// SPDX-License-Identifier: MIT
#pragma once
#include "SonyUfsContract.h"
typedef int (*SONY_PHYSICAL_READ)(SU64 Offset,unsigned char *Dst,SU64 Bytes);
static int SonyExtentValidate(const SONY_UFS_CONTEXT *C) {
 SU32 I,J;SU64 End;
 if(C->Magic!=SONY_UFS_MAGIC || C->Version!=1 || C->Size!=sizeof(*C) ||
    !C->FileCount || C->FileCount>SONY_UFS_FILES || !C->ExtentCount || C->ExtentCount>SONY_UFS_EXTENTS ||
    !C->PartitionFirst || !C->PartitionBlocks || C->DiskBlocks>0xffffffffULL ||
    C->PartitionFirst>=C->DiskBlocks || C->PartitionBlocks>C->DiskBlocks-C->PartitionFirst)return 0;
 for(I=0;I<C->FileCount;I++) {
  const SONY_UFS_FILE *F=&C->File[I];int Term=0;
  if(!F->Bytes || F->Bytes%4096 || !F->Count || F->First>=C->ExtentCount || F->Count>C->ExtentCount-F->First)return 0;
  for(J=0;J<sizeof(F->Name);J++) {
   unsigned char X=F->Name[J];if(!X){Term=J>0;break;}
   if(!((X>='a'&&X<='z')||(X>='0'&&X<='9')||X=='_'||X=='-'))return 0;
  }
  if(!Term)return 0;
  End=0;
  for(J=F->First;J<F->First+F->Count;J++) {
   const SONY_UFS_EXTENT *E=&C->Extent[J];SU64 N;
   if(E->Logical!=End || !E->Blocks || E->Lba<C->PartitionFirst ||
      E->Lba-C->PartitionFirst>=C->PartitionBlocks || E->Blocks>C->PartitionBlocks-(E->Lba-C->PartitionFirst))return 0;
   N=E->Blocks*4096;
   if(N>F->Bytes-End)return 0;
   End+=N;
  }
  if(End!=F->Bytes)return 0;
 }
 return 1;
}
static int SonyExtentRead(const SONY_UFS_CONTEXT *C,SU32 File,SU64 Offset,unsigned char *Dst,SU64 Bytes,SONY_PHYSICAL_READ Read) {
 const SONY_UFS_FILE *F;SU32 I;
 if(File>=C->FileCount)return -1;
 F=&C->File[File];
 if(Offset>F->Bytes || Bytes>F->Bytes-Offset)return -1;
 for(I=F->First;Bytes && I<F->First+F->Count;I++) {
  const SONY_UFS_EXTENT *E=&C->Extent[I];SU64 Size=E->Blocks*4096,Skip,Take;int Error;
  if(Offset<E->Logical || Offset-E->Logical>=Size)continue;
  Skip=Offset-E->Logical;Take=Size-Skip;if(Take>Bytes)Take=Bytes;
  Error=Read(E->Lba*4096+Skip,Dst,Take);if(Error)return Error;
  Offset+=Take;Dst+=Take;Bytes-=Take;
 }
 return Bytes?-1:0;
}
