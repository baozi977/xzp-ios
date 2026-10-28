// SPDX-License-Identifier: MIT
// boot_args ABI and return-slot convention from pinned xnuloader (see entry-plan.json).
typedef struct { UINT64 Base,Display,RowBytes,Width,Height,Depth; } XNU_VIDEO;
typedef struct {
  UINT16 Revision,Version;
  UINT64 VirtBase,PhysBase,MemSize,Top;
  XNU_VIDEO Video;
  UINT32 MachineType;
  UINT64 DeviceTreeP;
  UINT32 DeviceTreeLength;
  CHAR8 CommandLine[608];
  UINT64 BootFlags,MemSizeActual;
} XNU_BOOT_ARGS;
STATIC_ASSERT(OFFSET_OF(XNU_BOOT_ARGS,VirtBase)==8,"boot_args virtBase ABI");
STATIC_ASSERT(OFFSET_OF(XNU_BOOT_ARGS,DeviceTreeP)==96,"boot_args ADT ABI");
STATIC_ASSERT(sizeof(XNU_BOOT_ARGS)==736,"boot_args size ABI");
#define XNU_RAMDISK 0xa82e4000ULL
#define XNU_DT 0xa82ec000ULL
#define XNU_ARGS 0xa82fc000ULL
#define XNU_TOP 0xa8300000ULL
#define XNU_ENTRY 0xa745e4a8ULL
#include "NvramImage.h"
STATIC UINT8 *DtCursor;
STATIC UINT32 *DtStack[8];
STATIC UINTN DtDepth;
STATIC VOID DtProp(CONST CHAR8 *Name,CONST VOID *Value,UINT32 Size) {
  UINTN NameSize=AsciiStrLen(Name);
  ASSERT(DtDepth && NameSize<32 && DtCursor+36+ALIGN_VALUE(Size,4)<(UINT8 *)(UINTN)(XNU_DT+65536));
  ZeroMem(DtCursor,36+ALIGN_VALUE(Size,4));CopyMem(DtCursor,Name,NameSize);
  CopyMem(DtCursor+32,&Size,4);CopyMem(DtCursor+36,Value,Size);
  ++DtStack[DtDepth-1][0];DtCursor+=36+ALIGN_VALUE(Size,4);
}
STATIC VOID DtString(CONST CHAR8 *Name,CONST CHAR8 *Value) { DtProp(Name,Value,(UINT32)AsciiStrLen(Value)+1); }
STATIC VOID DtU32(CONST CHAR8 *Name,UINT32 Value) { DtProp(Name,&Value,4); }
STATIC VOID DtU64(CONST CHAR8 *Name,UINT64 Value) { DtProp(Name,&Value,8); }
STATIC VOID DtBegin(CONST CHAR8 *Name) {
  ASSERT(DtDepth<8);
  if (DtDepth) ++DtStack[DtDepth-1][1];
  DtStack[DtDepth++]=(UINT32 *)DtCursor;ZeroMem(DtCursor,8);DtCursor+=8;DtString("name",Name);
}
STATIC VOID DtEnd(VOID) { ASSERT(DtDepth);--DtDepth; }
STATIC EFI_STATUS PrepareXnuBoot(VOID) {
  UINTN I,J,Slots=0;UINT64 Midr,Frequency;
  UINT32 RootCrc;
  UINT64 Mmfr1;
  UINTN Patched=0,ReadPatched=0;
  UINT8 Seed[64];
  EFI_STATUS RootStatus;
  XNU_BOOT_ARGS *Args=(XNU_BOOT_ARGS *)(UINTN)XNU_ARGS;
  UINT64 Gic[4]={0x17a00000,0x10000,0x17b00000,0x100000};
  UINT64 Ranges[3]={0,0,0x200000};
  RootStatus=gBS->CalculateCrc32((VOID *)(UINTN)(SOURCE_BASE+SOURCE_SIZE),17024,&RootCrc);
  DEBUG((DEBUG_INFO,"S2_PID1_SOURCE_R21 crc=%x status=%r\n",RootCrc,RootStatus));
  if (EFI_ERROR(RootStatus)) return RootStatus;
  if (RootCrc!=0x78a2d4b1) return EFI_COMPROMISED_DATA;
  // The input CRC and all segment bounds were verified before this CPU-specific adaptation.
  // For this fixed kernel only: remove PAN operations when the CPU reports no PAN.
  __asm__ volatile("mrs %0,id_aa64mmfr1_el1":"=r"(Mmfr1));
  for (I=0;I<SegmentCount;I++) {
    MACH_SEG *S=&Segments[I];
    if (!(S->Prot&4)) continue;
    for(J=0;J+4<=S->FileSize;J+=4) {
      UINT32 *Op=(UINT32 *)(UINTN)(XNU_BASE+S->Vm-XNU_VIRTUAL+J);
      if (*Op==0xd5384268 && !((Mmfr1>>20)&15)) {
        // The PAN-enabled build asserts a nonzero PAN value in context-switch/copy helpers.
        // Match the omitted assertions of a PAN-less build without executing MRS PAN.
        DEBUG((DEBUG_INFO,"S2_PAN_READ_ADAPT_R21 pa=%p old=%x\n",Op,*Op));
        *Op=0xd2a00808;ReadPatched++;continue; // mov x8,#0x400000
      }
      if (*Op!=0xd500409f && *Op!=0xd500419f) continue;
      if (!((Mmfr1>>20)&15)) {
        DEBUG((DEBUG_INFO,"S2_PAN_NOP_R21 pa=%p old=%x\n",Op,*Op));
        *Op=0xd503201f;Patched++;
      }
    }
  }
  DEBUG((DEBUG_INFO,"S2_PAN_ADAPT_R21 feature=%lu patched=%lu\n",(Mmfr1>>20)&15,(UINT64)Patched));
  DEBUG((DEBUG_INFO,"S2_PAN_READ_COUNT_R21 patched=%lu\n",(UINT64)ReadPatched));
  if (!((Mmfr1>>20)&15) && (Patched!=24 || ReadPatched!=14)) return EFI_COMPROMISED_DATA;
  for (I=0;I<SegmentCount;I++) {
    MACH_SEG *S=&Segments[I];UINT64 Pa=XNU_BASE+S->Vm-XNU_VIRTUAL;
    for (J=0;J+32<=S->FileSize;J+=8) {
      UINT64 *Slot=(UINT64 *)(UINTN)(Pa+J);
      if (*Slot!=0x545352444c554e58ULL) continue;
      Slot[1]=(UINT64)(UINTN)loader_return_stub;Slot[2]=0;Slot[3]=XNU_BASE-XNU_VIRTUAL;Slots++;
      DEBUG((DEBUG_INFO,"S2_XNU_SLOT_R21 pa=%lx stub=%lx checkpoint=0\n",Pa+J,Slot[1]));
    }
  }
  if (Slots!=1) return EFI_COMPROMISED_DATA;
  __asm__ volatile("mrs %0,midr_el1":"=r"(Midr));
  __asm__ volatile("mrs %0,cntfrq_el0":"=r"(Frequency));
  if ((Midr&0xff00fff0)==0x4100d080) { Gic[0]=0x08000000;Gic[2]=0x080a0000; }
  else if (Midr!=0x51af8014 || Frequency!=19200000) return EFI_UNSUPPORTED;
  if (Midr==0x51af8014) {
    STATIC CONST struct { UINT64 Va;UINT32 Old,New; } TimerPatch[]={
      {0xfffffff00747493cULL,0x72a10008,0x72a00108},
      {0xfffffff007474944ULL,0x52a10008,0x52a00108},
      {0xfffffff007474968ULL,0x390e6d2a,0x390e4d2a},
      {0xfffffff0074749d4ULL,0x52800369,0x52800269},
      {0xfffffff007474b04ULL,0x71006e9f,0x71004e9f}
    };
    for (I=0;I<ARRAY_SIZE(TimerPatch);I++) {
      UINT32 *Op=(UINT32 *)(UINTN)(XNU_BASE+TimerPatch[I].Va-XNU_VIRTUAL);
      if (*Op!=TimerPatch[I].Old) return EFI_COMPROMISED_DATA;
      *Op=TimerPatch[I].New;
      DEBUG((DEBUG_INFO,"S2_TIMER_PATCH_R21 pa=%p old=%x new=%x\n",Op,TimerPatch[I].Old,TimerPatch[I].New));
    }
    DEBUG((DEBUG_INFO,"S2_TIMER_ADAPT_R21 intid=19 patched=5\n"));
  }
  // R21 diagnostic: leave recoverable user page faults unchanged and dump the
  // saved EL0 registers at the terminal user-abort path instead of SIGBUS.
  {
    STATIC CONST struct { UINT64 Va;UINT32 Old,New; } AbortPatch[]={
      {0xfffffff00745ca14ULL,0xd503201f,0xf9000be0},
      {0xfffffff00745cbf4ULL,0x910063e1,0xf9400be1},
      {0xfffffff00745cbf8ULL,0x52800020,0x90ffe220},
      {0xfffffff00745cbfcULL,0x52800042,0x9137fc00},
      {0xfffffff00745cc00ULL,0x97f9d4c6,0x9419e871},
    };
    CHAR8 *Message=(CHAR8 *)(UINTN)(XNU_BASE+0xfffffff0070a0dffULL-XNU_VIRTUAL);
    if (AsciiStrCmp(Message,"vm_fault() KERN_FAILURE from user fault on thread %p @%s:%d")) return EFI_COMPROMISED_DATA;
    ZeroMem(Message,50);CopyMem(Message,"XZPIOS R21 fatal user abort",sizeof("XZPIOS R21 fatal user abort"));
    for (I=0;I<ARRAY_SIZE(AbortPatch);I++) {
      UINT32 *Op=(UINT32 *)(UINTN)(XNU_BASE+AbortPatch[I].Va-XNU_VIRTUAL);
      if (*Op!=AbortPatch[I].Old) return EFI_COMPROMISED_DATA;
      *Op=AbortPatch[I].New;
    }
    DEBUG((DEBUG_INFO,"S2_USER_ABORT_DIAGNOSTIC_R21 patched=5\n"));
  }
  // XNU treats a zero SoC base as missing hardware and skips clock setup.
  // Use the measured GIC distributor base as the parent translation window.
  Ranges[1]=Gic[0];Gic[2]-=Ranges[1];Gic[0]=0;
  ZeroMem((VOID *)(UINTN)XNU_RAMDISK,0x8000);CopyMem((VOID *)(UINTN)XNU_RAMDISK,(VOID *)(UINTN)(SOURCE_BASE+SOURCE_SIZE),17024);
  ZeroMem((VOID *)(UINTN)XNU_DT,65536);DtCursor=(UINT8 *)(UINTN)XNU_DT;DtDepth=0;
  DtBegin("device-tree");DtString("compatible","sony,maple");DtString("model","SO-04J");DtString("platform-name","msm8998");DtU32("#address-cells",2);DtU32("#size-cells",2);
  for (I=0;I<sizeof(Seed);I++) {
    UINT64 Ticks;__asm__ volatile("mrs %0,cntvct_el0":"=r"(Ticks));Seed[I]=(UINT8)(Ticks>>(I&7))^(UINT8)(I*37);
  }
  DtBegin("chosen");DtProp("random-seed",Seed,sizeof(Seed));DtU64("dram-base",XNU_BASE);DtU64("dram-size",XNU_SIZE);DtString("psci-method",(Midr==0x51af8014)?"smc":"hvc");DtU32("debug-enabled",1);DtU32("embedded-panic-log-size",0x80000);DtU32("nvram-bank-size",8192);DtU32("nvram-bank-count",1);DtU32("nvram-current-bank",0);DtProp("nvram-proxy-data",NvramImage,sizeof(NvramImage));DtBegin("memory-map");{ UINT64 Rd[2]={XNU_RAMDISK,0x8000};DtProp("RAMDisk",Rd,sizeof(Rd)); }DtEnd();DtEnd();
  ZeroMem((VOID *)(UINTN)0xf1000000,0x80000);
  DtBegin("pram");{ UINT64 Pram[2]={0xf1000000,0x80000};DtProp("reg",Pram,sizeof(Pram)); }DtEnd();
  DtBegin("defaults");DtU32("kern.vm_compressor",2);DtEnd();
  DtBegin("options");DtEnd();
  DtBegin("cpus");DtBegin("cpu0");DtString("device_type","cpu");DtString("state","running");DtU32("reg",0);DtU32("cpu-id",0);DtU64("timebase-frequency",Frequency);DtU64("fixed-frequency",Frequency);DtU64("clock-frequency",1900800000);DtEnd();DtEnd();
  DtBegin("arm-io");DtString("device_type","msm8998");DtProp("ranges",Ranges,sizeof(Ranges));DtU32("#address-cells",2);DtU32("#size-cells",2);
  DtBegin("gic");DtString("device_type","interrupt-controller");DtString("compatible","arm,gic-v3");DtProp("reg",Gic,sizeof(Gic));DtEnd();DtEnd();DtEnd();
  ZeroMem(Args,sizeof(*Args));Args->Revision=2;Args->Version=2;Args->VirtBase=XNU_VIRTUAL;Args->PhysBase=XNU_BASE;Args->MemSize=XNU_SIZE;Args->MemSizeActual=XNU_SIZE;Args->Top=XNU_TOP;
  Args->DeviceTreeP=XNU_DT-XNU_BASE+XNU_VIRTUAL;Args->DeviceTreeLength=(UINT32)(DtCursor-(UINT8 *)(UINTN)XNU_DT);
  CopyMem(Args->CommandLine,"debug=0x8 cpus=1 rd=md0 loader_return_after=15",sizeof("debug=0x8 cpus=1 rd=md0 loader_return_after=15"));
  DEBUG((DEBUG_INFO,"S2_XNU_BOOT_R21 entry=%lx args=%lx dt=%lx size=%u top=%lx gicd=%lx gicr=%lx cntfrq=%lx\n",XNU_ENTRY,XNU_ARGS,XNU_DT,Args->DeviceTreeLength,XNU_TOP,Gic[0]+Ranges[1],Gic[2]+Ranges[1],Frequency));
  return EFI_SUCCESS;
}
STATIC VOID FlushXnu(VOID) {
  UINT64 Address,Ctr,Line;
  __asm__ volatile("mrs %0,ctr_el0":"=r"(Ctr));Line=4ULL<<((Ctr>>16)&15);
  for(Address=XNU_BASE;Address<XNU_TOP;Address+=Line) __asm__ volatile("dc civac,%0"::"r"(Address):"memory");
  __asm__ volatile("dsb sy":::"memory");
}
