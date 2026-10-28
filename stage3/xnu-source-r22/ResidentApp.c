// SPDX-License-Identifier: MIT
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/SerialPortLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include "LoaderContext.h"
#include "LoaderJump.h"
#include "KernelStage.h"
#include "XnuBoot.h"
#define GIC_STAGE_TAG "_R22"
#include "GicPrepare.h"

VOID resident_payload_stub (VOID);
STATIC UINT8 MapBuffer[65536];
STATIC UINT64 SavedTcr, SavedSctlr;
UINT64 KernelEntryState[6];

STATIC VOID CleanArena (VOID) {
  UINTN Address, Line;
  UINT64 Ctr;
  __asm__ volatile ("mrs %0, ctr_el0" : "=r" (Ctr));
  Line = 4U << ((Ctr >> 16) & 15);
  for (Address=0xa0000000; Address<0xa4000000; Address+=Line)
    __asm__ volatile ("dc cvac, %0" :: "r" (Address) : "memory");
  __asm__ volatile ("dsb sy" ::: "memory");
}

STATIC EFI_STATUS CheckMap (CONST CHAR8 *Phase) {
  UINTN Size=sizeof(MapBuffer), Key=0, DescSize=0, Offset;
  UINT32 Version=0;
  EFI_STATUS Status;
  EFI_MEMORY_DESCRIPTOR *Desc;
  Status=gBS->GetMemoryMap (&Size,(EFI_MEMORY_DESCRIPTOR *)MapBuffer,&Key,&DescSize,&Version);
  DEBUG ((DEBUG_INFO,"S2_MAP %a status=%r bytes=%lx key=%lx desc=%lx version=%u\n",Phase,Status,Size,Key,DescSize,Version));
  if (EFI_ERROR(Status) || DescSize<sizeof(EFI_MEMORY_DESCRIPTOR) || Size%DescSize) return EFI_COMPROMISED_DATA;
  for (Offset=0;Offset<Size;Offset+=DescSize) {
    UINT64 End;
    Desc=(EFI_MEMORY_DESCRIPTOR *)(MapBuffer+Offset);
    End=Desc->PhysicalStart+EFI_PAGES_TO_SIZE(Desc->NumberOfPages);
    DEBUG ((DEBUG_INFO,"S2_MEM type=%u base=%lx end=%lx attr=%lx\n",Desc->Type,Desc->PhysicalStart,End,Desc->Attribute));
    if (End<Desc->PhysicalStart) return EFI_COMPROMISED_DATA;
    if (Desc->Type==EfiConventionalMemory && (Desc->PhysicalStart<0xa0200000 || End>0xa4000000)) return EFI_COMPROMISED_DATA;
  }
  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI ResidentAppEntry (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  EFI_STATUS Status;
  EFI_PHYSICAL_ADDRESS Address, Scratch;
  UINT64 Sre,Tcr,Sctlr,Midr;
  UINTN Index;
  DEBUG ((DEBUG_INFO,"XZPIOS_S2_APP_ENTER_R22 handle=%p bs=%p\n",ImageHandle,SystemTable->BootServices));
  Status=StageKernel();
  if (EFI_ERROR(Status)) return Status;
  Status=CheckMap("before");
  if (EFI_ERROR(Status)) return Status;
  Address=0xffcc0000;
  Status=gBS->AllocatePages(AllocateAddress,EfiLoaderData,1,&Address);
  DEBUG ((DEBUG_INFO,"S2_PROTECTED_ALLOC status=%r\n",Status));
  if (Status!=EFI_NOT_FOUND) return EFI_COMPROMISED_DATA;
  Address=0xa1000000;
  Status=gBS->AllocatePages(AllocateAddress,EfiLoaderData,2048,&Address);
  DEBUG ((DEBUG_INFO,"S2_WINDOW_ALLOC status=%r base=%lx bytes=800000\n",Status,Address));
  if (EFI_ERROR(Status) || Address!=0xa1000000) return EFI_OUT_OF_RESOURCES;
  for (Index=0;Index<64;Index++) {
    ((volatile UINT64 *)(UINTN)Address)[Index]=0x5352494f53000000ULL+Index;
    ((volatile UINT64 *)(UINTN)(Address+0x800000-512))[Index]=0x5352494f53100000ULL+Index;
  }
  __asm__ volatile ("mrs %0, S3_0_C12_C12_5" : "=r"(Sre));
  if (!(Sre&1)) {
    Sre|=1;
    __asm__ volatile("msr S3_0_C12_C12_5,%0\nisb"::"r"(Sre):"memory");
    __asm__ volatile("mrs %0,S3_0_C12_C12_5":"=r"(Sre));
  }
  DEBUG ((DEBUG_INFO,"S2_ICC_SRE=%lx\n",Sre));
  if (!(Sre&1)) return EFI_UNSUPPORTED;
  __asm__ volatile("mrs %0,tcr_el1":"=r"(SavedTcr));
  __asm__ volatile("mrs %0,sctlr_el1":"=r"(SavedSctlr));
  Status=PrepareXnuBoot();
  if (EFI_ERROR(Status)) return Status;
  Status=PrepareGicForKernel();
  if (EFI_ERROR(Status)) return Status;
  DEBUG ((DEBUG_INFO,"S2_HANDOFF_BEGIN_R22 TCR=%lx SCTLR=%lx\n",SavedTcr,SavedSctlr));
  if (loader_save_ctx()==0) {
    if (loader_ctx[CTX_SP]<0xa3fc0000 || loader_ctx[CTX_SP]>=0xa4000000) return EFI_COMPROMISED_DATA;
    CleanArena();
    FlushXnu();
    jump_to_kernel(XNU_ENTRY,XNU_ARGS);
  }
  __asm__ volatile("mrs %0,tcr_el1":"=r"(Tcr));
  __asm__ volatile("mrs %0,sctlr_el1":"=r"(Sctlr));
  DEBUG ((DEBUG_INFO,"S2_XNU_RETURN_R22 reason=%lx code=%lx elr=%lx far=%lx lr=%lx sp=%lx\n",
    loader_ctx[CTX_REASON],loader_ctx[CTX_ESR],loader_ctx[CTX_ELR],loader_ctx[CTX_FAR],loader_ctx[CTX_LR],loader_ctx[CTX_KSP]));
  DEBUG ((DEBUG_INFO,"S2_RESTORED TCR=%lx SCTLR=%lx\n",Tcr,Sctlr));
  if (loader_ctx[CTX_LOG_PA]>=XNU_BASE && loader_ctx[CTX_LOG_PA]<XNU_BASE+XNU_SIZE && loader_ctx[CTX_LOG_LEN]<=XNU_BASE+XNU_SIZE-loader_ctx[CTX_LOG_PA]) {
    UINTN Length=(UINTN)loader_ctx[CTX_LOG_LEN];
    if (Length>0x20000) Length=0x20000;
    DEBUG ((DEBUG_INFO,"S2_XNU_LOG_BEGIN_R22 bytes=%lu\n",(UINT64)Length));
    SerialPortWrite((UINT8 *)(UINTN)loader_ctx[CTX_LOG_PA],Length);
    DEBUG ((DEBUG_INFO,"\nS2_XNU_LOG_END_R22\n"));
  }
  DEBUG ((DEBUG_INFO,"S2_KERNEL_REGS_R22 SCTLR=%lx TCR=%lx TTBR0=%lx TTBR1=%lx MAIR=%lx MMFR0=%lx\n",KernelEntryState[0],KernelEntryState[1],KernelEntryState[2],KernelEntryState[3],KernelEntryState[4],KernelEntryState[5]));
  if (loader_ctx[CTX_REASON]!=0 ||
      Tcr!=SavedTcr || Sctlr!=SavedSctlr || !SystemTable->BootServices) return EFI_COMPROMISED_DATA;
  for (Index=0;Index<64;Index++) {
    if (((volatile UINT64 *)0xa1000000)[Index]!=0x5352494f53000000ULL+Index ||
        ((volatile UINT64 *)(0xa1800000-512))[Index]!=0x5352494f53100000ULL+Index) return EFI_COMPROMISED_DATA;
  }
  Status=CheckMap("after");
  if (EFI_ERROR(Status)) return Status;
  Scratch=0;
  Status=gBS->AllocatePages(AllocateAnyPages,EfiLoaderData,1,&Scratch);
  DEBUG ((DEBUG_INFO,"S2_POST_RETURN_ALLOC status=%r base=%lx\n",Status,Scratch));
  if (EFI_ERROR(Status)) return Status;
  Status=gBS->FreePages(Scratch,1);
  if (EFI_ERROR(Status)) return Status;
  Status=gBS->FreePages(0xa1000000,2048);
  if (EFI_ERROR(Status)) return Status;
  DEBUG ((DEBUG_INFO,"XZPIOS_S2_XNU_RETURN_PASS_R22\n"));
  return EFI_SUCCESS;
}
