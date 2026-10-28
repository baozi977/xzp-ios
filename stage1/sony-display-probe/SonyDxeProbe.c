// SPDX-License-Identifier: MIT
#include <Uefi.h>
#include <Library/ArmSmcLib.h>
#include <Library/DebugLib.h>
#include <Library/SerialPortLib.h>
#include <Library/TimerLib.h>
#include <Library/IoLib.h>


STATIC VOID ReadBlock (CONST CHAR8 *Name, UINTN Base, CONST UINT32 *Offsets, UINTN Count) {
  UINTN Index;
  for (Index = 0; Index < Count; Index++) {
    UINT32 Value = MmioRead32 (Base + Offsets[Index]);
    DEBUG ((DEBUG_INFO, "MDP_READ %a base=%08lx reg=%03x value=%08x\n", Name, Base, Offsets[Index], Value));
  }
}

STATIC VOID CaptureDisplay (VOID) {
  // Register block bases come from the running SO-04J DTB, not another handset.
  STATIC CONST UINT32 PipeBases[] = {0x5000,0x7000,0x9000,0xb000,0x25000,0x27000,0x29000,0x2b000};
  STATIC CONST UINT32 PipeRegs[] = {0,4,8,0xc,0x10,0x14,0x18,0x1c,0x20,0x24,0x28,0x30,0x34,0x38,0x70,0xa4};
  STATIC CONST UINT32 CtlBases[] = {0x2000,0x2200,0x2400,0x2600,0x2800};
  STATIC CONST UINT32 CtlRegs[] = {0,4,8,0xc,0x10,0x14,0x40,0x44,0x48,0x4c,0x50,0x70,0x74,0x78,0x7c,0x80};
  STATIC CONST UINT32 MixerBases[] = {0x45000,0x46000,0x47000,0x4a000};
  STATIC CONST UINT32 MixerRegs[] = {0,4};
  STATIC CONST UINT32 IntfBases[] = {0x6b000,0x6b800,0x6c000,0x6c800};
  STATIC CONST UINT32 IntfRegs[] = {0,4,8,0xc,0x14,0x1c,0x24,0x3c};
  UINTN Index;
  DEBUG ((DEBUG_INFO, "SONY_DISPLAY_BEGIN_20261027_B HW=%08x\n", MmioRead32 (0x0c900000)));
  for (Index=0; Index<ARRAY_SIZE(PipeBases); Index++) ReadBlock ("SSPP",0x0c900000+PipeBases[Index],PipeRegs,ARRAY_SIZE(PipeRegs));
  for (Index=0; Index<ARRAY_SIZE(CtlBases); Index++) ReadBlock ("CTL",0x0c900000+CtlBases[Index],CtlRegs,ARRAY_SIZE(CtlRegs));
  for (Index=0; Index<ARRAY_SIZE(MixerBases); Index++) ReadBlock ("LM",0x0c900000+MixerBases[Index],MixerRegs,ARRAY_SIZE(MixerRegs));
  for (Index=0; Index<ARRAY_SIZE(IntfBases); Index++) ReadBlock ("INTF",0x0c900000+IntfBases[Index],IntfRegs,ARRAY_SIZE(IntfRegs));
  DEBUG ((DEBUG_INFO, "SONY_DISPLAY_END_20261027_B\n"));
}

EFI_STATUS EFIAPI SonyDxeProbeEntry (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  ARM_SMC_ARGS Args = {0};
  UINT64 Midr, Tcr, Sctlr;
  STATIC UINT8 Marker[] = "XZPIOS_SONY_UEFI_DXE_REACHED_20261027_B\n";
  STATIC UINT8 Reset[] = "XZPIOS_SONY_UEFI_RESET_20261027_B\n";
  SerialPortWrite (Marker, sizeof (Marker) - 1);
  DEBUG ((DEBUG_INFO, "Sony DXE: EFI revision %x, table %p, boot services %p\n",
    SystemTable->Hdr.Revision, SystemTable, SystemTable->BootServices));
  __asm__ volatile ("mrs %0, midr_el1" : "=r" (Midr));
  __asm__ volatile ("mrs %0, tcr_el1" : "=r" (Tcr));
  __asm__ volatile ("mrs %0, sctlr_el1" : "=r" (Sctlr));
  DEBUG ((DEBUG_INFO, "SONY_DXE_REGS MIDR=%lx TCR=%lx SCTLR=%lx\n", Midr, Tcr, Sctlr));
  MicroSecondDelay (1000000);
  if ((Midr & 0xff00fff0) == 0x4100d080) {
    STATIC UINT8 Stop[] = "XZPIOS_SONY_UEFI_QEMU_STOP_BEFORE_SCM\n";
    SerialPortWrite (Stop, sizeof (Stop) - 1);
    for (;;) __asm__ volatile ("wfe");
  }
  if (Midr != 0x51af8014) {
    STATIC UINT8 WrongCpu[] = "XZPIOS_SONY_UEFI_UNEXPECTED_CPU\n";
    SerialPortWrite (WrongCpu, sizeof (WrongCpu) - 1);
    for (;;) __asm__ volatile ("wfe");
  }
  CaptureDisplay ();
  SerialPortWrite (Reset, sizeof (Reset) - 1);
  Args.Arg0 = 0xc2000901; Args.Arg1 = 1;
  ArmCallSmc (&Args);
  Args.Arg0 = 0xc2000902; Args.Arg1 = 1;
  Args.Arg2 = Args.Arg3 = Args.Arg4 = Args.Arg5 = Args.Arg6 = Args.Arg7 = 0;
  ArmCallSmc (&Args);
  *(volatile UINT32 *)0x010ac000 = 0;
  __asm__ volatile ("dsb sy" ::: "memory");
  for (;;) __asm__ volatile ("wfe");
}
