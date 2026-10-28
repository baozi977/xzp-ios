// SPDX-License-Identifier: MIT
// Validate and stage the source-built Mach-O before handoff.
#define XNU_BASE 0xa4000000ULL
#define XNU_SIZE 0x10000000ULL
#define XNU_VIRTUAL 0xfffffff004000000ULL
#define SOURCE_BASE 0xf0000000ULL
#include "KernelContract.h"
typedef struct { UINT32 Cmd,Size; CHAR8 Name[16]; UINT64 Vm,VmSize,File,FileSize; UINT32 MaxProt,Prot,Sections,Flags; } MACH_SEG;
STATIC MACH_SEG Segments[32];
STATIC UINTN SegmentCount;

STATIC BOOLEAN VerifyKernel (VOID) {
  UINTN I,J;
  for (I=0;I<SegmentCount;I++) {
    MACH_SEG *S=&Segments[I];
    UINT8 *Dest=(UINT8 *)(UINTN)(XNU_BASE+S->Vm-XNU_VIRTUAL);
    if (CompareMem(Dest,(VOID *)(UINTN)(SOURCE_BASE+S->File),(UINTN)S->FileSize)) return FALSE;
    for (J=(UINTN)S->FileSize;J<S->VmSize;J++) if (Dest[J]) return FALSE;
  }
  return TRUE;
}

STATIC EFI_STATUS StageKernel (VOID) {
  UINT32 *Header=(UINT32 *)(UINTN)SOURCE_BASE;
  UINT32 Crc=0;
  UINTN I,J,Offset=32,MapSize=65536,Key,DescSize;
  UINT32 Version;
  STATIC UINT8 Map[65536];
  UINT64 Owned=0;
  EFI_STATUS Status;
  EFI_PHYSICAL_ADDRESS Protected;
  DEBUG ((DEBUG_INFO,"S2_XNU_WINDOW_U07 base=%lx end=%lx source=%lx bytes=%u\n",XNU_BASE,XNU_BASE+XNU_SIZE,SOURCE_BASE,SOURCE_SIZE));
  Status=gBS->GetMemoryMap(&MapSize,(EFI_MEMORY_DESCRIPTOR *)Map,&Key,&DescSize,&Version);
  if (EFI_ERROR(Status) || DescSize<sizeof(EFI_MEMORY_DESCRIPTOR) || MapSize%DescSize) return EFI_COMPROMISED_DATA;
  for (I=0;I<MapSize;I+=DescSize) {
    EFI_MEMORY_DESCRIPTOR *D=(EFI_MEMORY_DESCRIPTOR *)(Map+I);
    UINT64 End=D->PhysicalStart+EFI_PAGES_TO_SIZE(D->NumberOfPages);
    if (End>XNU_BASE && D->PhysicalStart<XNU_BASE+XNU_SIZE) {
      if (D->Type!=EfiLoaderData || D->PhysicalStart<XNU_BASE || End>XNU_BASE+XNU_SIZE) return EFI_COMPROMISED_DATA;
      Owned+=End-D->PhysicalStart;
    }
  }
  if (Owned!=XNU_SIZE) return EFI_COMPROMISED_DATA;
  Protected=XNU_BASE;
  Status=gBS->AllocatePages(AllocateAddress,EfiLoaderData,1,&Protected);
  if (Status!=EFI_NOT_FOUND) return EFI_COMPROMISED_DATA;
  // Distinct word per 4 KiB page detects unmapped pages and address aliasing.
  for (I=0;I<XNU_SIZE;I+=4096) *(volatile UINT64 *)(UINTN)(XNU_BASE+I)=0x585a50494f530000ULL^(XNU_BASE+I);
  for (I=0;I<XNU_SIZE;I+=4096) if (*(volatile UINT64 *)(UINTN)(XNU_BASE+I)!=(0x585a50494f530000ULL^(XNU_BASE+I))) return EFI_DEVICE_ERROR;
  DEBUG ((DEBUG_INFO,"S2_XNU_PAGE_SWEEP_PASS_U07 pages=65536\n"));
  Status=gBS->CalculateCrc32((VOID *)(UINTN)SOURCE_BASE,SOURCE_SIZE,&Crc);
  DEBUG ((DEBUG_INFO,"S2_KERNEL_SOURCE_U07 crc=%x status=%r\n",Crc,Status));
  if (EFI_ERROR(Status) || Crc!=SOURCE_CRC32 || Header[0]!=0xfeedfacf || Header[1]!=0x100000c || Header[3]!=2 || Header[5]>SOURCE_SIZE-32 || Header[4]>4096) return EFI_COMPROMISED_DATA;
  SegmentCount=0;
  // Validate the entire load-command table before writing any segment.
  for (I=0;I<Header[4];I++) {
    UINT32 *Cmd=(UINT32 *)(UINTN)(SOURCE_BASE+Offset);
    if (Offset>32+Header[5]-8 || Cmd[1]<8 || Cmd[1]>32+Header[5]-Offset) return EFI_COMPROMISED_DATA;
    if (Cmd[0]==0x19) {
      MACH_SEG *S;
      if (Cmd[1]<sizeof(MACH_SEG) || SegmentCount>=32) return EFI_COMPROMISED_DATA;
      S=&Segments[SegmentCount];CopyMem(S,Cmd,sizeof(*S));
      if (S->Vm<XNU_VIRTUAL || S->Vm-XNU_VIRTUAL>XNU_SIZE || S->VmSize>XNU_SIZE-(S->Vm-XNU_VIRTUAL) || S->File>SOURCE_SIZE || S->FileSize>SOURCE_SIZE-S->File || S->FileSize>S->VmSize) return EFI_COMPROMISED_DATA;
      for (J=0;J<SegmentCount;J++) if (S->Vm<Segments[J].Vm+Segments[J].VmSize && S->Vm+S->VmSize>Segments[J].Vm) return EFI_COMPROMISED_DATA;
      SegmentCount++;
    }
    Offset+=Cmd[1];
  }
  if (Offset!=32+Header[5] || SegmentCount!=XNU_SEGMENT_COUNT) return EFI_COMPROMISED_DATA;
  for (I=0;I<SegmentCount;I++) {
    MACH_SEG *S=&Segments[I];
    VOID *Dest=(VOID *)(UINTN)(XNU_BASE+S->Vm-XNU_VIRTUAL);
    SetMem(Dest,(UINTN)S->VmSize,0);
    CopyMem(Dest,(VOID *)(UINTN)(SOURCE_BASE+S->File),(UINTN)S->FileSize);
    DEBUG ((DEBUG_INFO,"S2_XNU_SEG_U07 index=%u dest=%p bytes=%lx file=%lx\n",(UINT32)I,Dest,S->VmSize,S->FileSize));
  }
  if (!VerifyKernel()) return EFI_COMPROMISED_DATA;
  DEBUG ((DEBUG_INFO,"S2_KERNEL_COPY_BSS_PASS_U07 segments=%u\n",(UINT32)SegmentCount));
  return EFI_SUCCESS;
}
