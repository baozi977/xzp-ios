// SPDX-License-Identifier: MIT
// boot_args ABI and return-slot convention from pinned xnuloader (see entry-plan.json).
#include <Library/PrintLib.h>
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
  UINTN PanInstructions=0;UINT32 TimerIntid=19;
  UINT8 Seed[64];
  EFI_STATUS RootStatus;
  XNU_BOOT_ARGS *Args=(XNU_BOOT_ARGS *)(UINTN)XNU_ARGS;
  UINT64 Gic[4]={0x17a00000,0x10000,0x17b00000,0x100000};
  UINT64 Ranges[3]={0,0,0x200000};
  RootStatus=gBS->CalculateCrc32((VOID *)(UINTN)(SOURCE_BASE+SOURCE_SIZE),19456,&RootCrc);
  DEBUG((DEBUG_INFO,"S2_PID1_SOURCE_S4J crc=%x status=%r\n",RootCrc,RootStatus));
  if (EFI_ERROR(RootStatus)) return RootStatus;
  if (RootCrc!=0x17ecc09e) return EFI_COMPROMISED_DATA;
  // Source code disables PAN; audit the staged executable bytes without edits.
  __asm__ volatile("mrs %0,id_aa64mmfr1_el1":"=r"(Mmfr1));
  for (I=0;I<SegmentCount;I++) {
    MACH_SEG *S=&Segments[I];
    if (!(S->Prot&4)) continue;
    for(J=0;J+4<=S->FileSize;J+=4) {
      UINT32 Op=*(UINT32 *)(UINTN)(XNU_BASE+S->Vm-XNU_VIRTUAL+J);
      if (Op==0xd500409f || Op==0xd500419f ||
          (Op&0xffffffe0)==0xd5384260 || (Op&0xffffffe0)==0xd5184260) PanInstructions++;
    }
  }
  DEBUG((DEBUG_INFO,"S3_SOURCE_NO_PAN_S4J feature=%lu instructions=%lu\n",(Mmfr1>>20)&15,(UINT64)PanInstructions));
  if (PanInstructions) return EFI_UNSUPPORTED;
  for (I=0;I<SegmentCount;I++) {
    MACH_SEG *S=&Segments[I];UINT64 Pa=XNU_BASE+S->Vm-XNU_VIRTUAL;
    for (J=0;J+32<=S->FileSize;J+=8) {
      UINT64 *Slot=(UINT64 *)(UINTN)(Pa+J);
      if (*Slot!=0x545352444c554e58ULL) continue;
      Slot[1]=(UINT64)(UINTN)loader_return_stub;Slot[2]=0;Slot[3]=XNU_BASE-XNU_VIRTUAL;Slots++;
      DEBUG((DEBUG_INFO,"S2_XNU_SLOT_S4J pa=%lx stub=%lx checkpoint=0\n",Pa+J,Slot[1]));
    }
  }
  if (Slots!=1) return EFI_COMPROMISED_DATA;
  __asm__ volatile("mrs %0,midr_el1":"=r"(Midr));
  __asm__ volatile("mrs %0,cntfrq_el0":"=r"(Frequency));
  if ((Midr&0xff00fff0)==0x4100d080) { Gic[0]=0x08000000;Gic[2]=0x080a0000;TimerIntid=27; }
  else if (Midr!=0x51af8014 || Frequency!=19200000) return EFI_UNSUPPORTED;
  // XNU treats a zero SoC base as missing hardware and skips clock setup.
  // Use the measured GIC distributor base as the parent translation window.
  Ranges[1]=Gic[0];Gic[2]-=Ranges[1];Gic[0]=0;
  ZeroMem((VOID *)(UINTN)XNU_RAMDISK,XNU_RAMDISK_SIZE);CopyMem((VOID *)(UINTN)XNU_RAMDISK,(VOID *)(UINTN)(SOURCE_BASE+SOURCE_SIZE),19456);
  ZeroMem((VOID *)(UINTN)XNU_DT,65536);DtCursor=(UINT8 *)(UINTN)XNU_DT;DtDepth=0;
  DtBegin("device-tree");DtString("compatible","sony,maple");DtString("model","SO-04J");DtString("platform-name","msm8998");DtU32("#address-cells",2);DtU32("#size-cells",2);
  for (I=0;I<sizeof(Seed);I++) {
    UINT64 Ticks;__asm__ volatile("mrs %0,cntvct_el0":"=r"(Ticks));Seed[I]=(UINT8)(Ticks>>(I&7))^(UINT8)(I*37);
  }
  DtBegin("chosen");DtProp("random-seed",Seed,sizeof(Seed));DtU64("dram-base",XNU_BASE);DtU64("dram-size",XNU_SIZE);DtString("psci-method",(Midr==0x51af8014)?"smc":"hvc");DtU32("debug-enabled",1);DtU32("embedded-panic-log-size",0x80000);DtU32("nvram-bank-size",8192);DtU32("nvram-bank-count",1);DtU32("nvram-current-bank",0);DtProp("nvram-proxy-data",NvramImage,sizeof(NvramImage));DtBegin("memory-map");{ UINT64 Rd[2]={XNU_RAMDISK,XNU_RAMDISK_SIZE};DtProp("RAMDisk",Rd,sizeof(Rd)); }DtEnd();DtEnd();
  ZeroMem((VOID *)(UINTN)0xf1000000,0x80000);
  DtBegin("pram");{ UINT64 Pram[2]={0xf1000000,0x80000};DtProp("reg",Pram,sizeof(Pram)); }DtEnd();
  DtBegin("defaults");DtU32("kern.vm_compressor",2);DtEnd();
  DtBegin("options");DtEnd();
  DtBegin("cpus");DtBegin("cpu0");DtString("device_type","cpu");DtString("state","running");DtU32("reg",0);DtU32("cpu-id",0);DtU64("timebase-frequency",Frequency);DtU64("fixed-frequency",Frequency);DtU64("clock-frequency",1900800000);DtEnd();DtEnd();
  DtBegin("arm-io");DtString("device_type","msm8998");DtProp("ranges",Ranges,sizeof(Ranges));DtU32("#address-cells",2);DtU32("#size-cells",2);
  DtBegin("gic");DtString("device_type","interrupt-controller");DtString("compatible","arm,gic-v3");DtProp("reg",Gic,sizeof(Gic));DtEnd();
  DtBegin("timer");DtU32("p8,timer-intid",TimerIntid);DtEnd();DtEnd();DtEnd();
  DEBUG((DEBUG_INFO,"S3_SOURCE_TIMER_DT_S4J intid=%u\n",TimerIntid));
  ZeroMem(Args,sizeof(*Args));Args->Revision=2;Args->Version=2;Args->VirtBase=XNU_VIRTUAL;Args->PhysBase=XNU_BASE;Args->MemSize=XNU_SIZE;Args->MemSizeActual=XNU_SIZE;Args->Top=XNU_TOP;
  Args->DeviceTreeP=XNU_DT-XNU_BASE+XNU_VIRTUAL;Args->DeviceTreeLength=(UINT32)(DtCursor-(UINT8 *)(UINTN)XNU_DT);
  CopyMem(Args->CommandLine,"debug=0x8 cpus=1 rd=md0 loader_return_after=15",sizeof("debug=0x8 cpus=1 rd=md0 loader_return_after=15"));
  if(SonyUfsContextPa)AsciiSPrint(Args->CommandLine,sizeof(Args->CommandLine),"debug=0x8 cpus=1 rd=md0 loader_return_after=90 epoch=1793174000 sony_ufs_context=0x%lx ufs_native_slots=1 ufs_native_buffer=32768",SonyUfsContextPa);
  DEBUG((DEBUG_INFO,"S2_XNU_BOOT_S4J entry=%lx args=%lx dt=%lx size=%u top=%lx gicd=%lx gicr=%lx cntfrq=%lx\n",XNU_ENTRY,XNU_ARGS,XNU_DT,Args->DeviceTreeLength,XNU_TOP,Gic[0]+Ranges[1],Gic[2]+Ranges[1],Frequency));
  return EFI_SUCCESS;
}
STATIC VOID FlushXnu(VOID) {
  UINT64 Address,Ctr,Line;
  __asm__ volatile("mrs %0,ctr_el0":"=r"(Ctr));Line=4ULL<<((Ctr>>16)&15);
  for(Address=XNU_BASE;Address<XNU_TOP;Address+=Line) __asm__ volatile("dc civac,%0"::"r"(Address):"memory");
  __asm__ volatile("dsb sy":::"memory");
}
