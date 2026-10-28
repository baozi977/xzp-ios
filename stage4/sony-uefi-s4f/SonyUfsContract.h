// SPDX-License-Identifier: MIT
// Shared, fixed-width UEFI/XNU ABI; all geometry comes from this SO-04J.
#pragma once
typedef unsigned long long SU64;
typedef unsigned int SU32;
#define SONY_UFS_MAGIC 0x314653554e4f5358ULL
#define SONY_UFS_FILES 4
#define SONY_UFS_EXTENTS 128
typedef struct {SU64 Bytes;SU32 First,Count;char Name[24];SU64 ProofFnv;} SONY_UFS_FILE;
typedef struct {SU64 Logical,Lba,Blocks;} SONY_UFS_EXTENT;
typedef struct {
 SU64 Magic;SU32 Version,Size;SU64 Hc,Area;
 SU32 AreaSize,UcdOffset,UcdSize,Slots,OldLo,OldHi,OldRun,FileCount;
 volatile SU32 TakeBack,ListIn,SwitchIn,SwitchOut,Cnr;
 SU32 ExtentCount;SU64 PartitionFirst,PartitionBlocks,DiskBlocks;SU32 Reserved[4];
 SONY_UFS_FILE File[SONY_UFS_FILES];SONY_UFS_EXTENT Extent[SONY_UFS_EXTENTS];
 unsigned char Padding[704];
} SONY_UFS_CONTEXT;
_Static_assert(sizeof(SONY_UFS_FILE)==48,"Sony file ABI");
_Static_assert(__builtin_offsetof(SONY_UFS_CONTEXT,File)==128,"Sony table ABI");
_Static_assert(__builtin_offsetof(SONY_UFS_CONTEXT,Extent)==320,"Sony extent ABI");
_Static_assert(sizeof(SONY_UFS_CONTEXT)==4096,"Sony context page ABI");
