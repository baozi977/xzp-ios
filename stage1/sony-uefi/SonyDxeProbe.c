// SPDX-License-Identifier: MIT
#include <Uefi.h>
#include <Library/ArmSmcLib.h>
#include <Library/DebugLib.h>
#include <Library/SerialPortLib.h>
#include <Library/TimerLib.h>

EFI_STATUS EFIAPI SonyDxeProbeEntry (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  ARM_SMC_ARGS Args = {0};
  UINT64 Midr, Tcr, Sctlr;
  STATIC UINT8 Marker[] = "XZPIOS_SONY_UEFI_DXE_REACHED_20261027_A\n";
  STATIC UINT8 Reset[] = "XZPIOS_SONY_UEFI_RESET_20261027_A\n";
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
