// SPDX-License-Identifier: MIT
#include <Uefi.h>
#include <Protocol/DebugSupport.h>
#include <Library/ArmSmcLib.h>
#include <Library/PrintLib.h>
#include <Library/SerialPortLib.h>

VOID DefaultExceptionHandler (EFI_EXCEPTION_TYPE ExceptionType, EFI_SYSTEM_CONTEXT SystemContext) {
  CHAR8 Buffer[320];
  UINTN Count;
  UINT64 Midr;
  ARM_SMC_ARGS Args={0};
  EFI_SYSTEM_CONTEXT_AARCH64 *Cpu=SystemContext.SystemContextAArch64;
  Count=AsciiSPrint(Buffer,sizeof(Buffer),"XZPIOS_S2_NATIVE_EXCEPTION_S4J type=%lx ESR=%lx ELR=%lx FAR=%lx SPSR=%lx SP=%lx LR=%lx X0=%lx X1=%lx\n",
    (UINT64)ExceptionType,Cpu->ESR,Cpu->ELR,Cpu->FAR,Cpu->SPSR,Cpu->SP,Cpu->LR,Cpu->X0,Cpu->X1);
  SerialPortWrite((UINT8 *)Buffer,Count);
  __asm__ volatile("mrs %0,midr_el1":"=r"(Midr));
  if (Midr==0x51af8014) {
    Args.Arg0=0xc2000901;Args.Arg1=1;ArmCallSmc(&Args);
    Args.Arg0=0xc2000902;Args.Arg1=1;Args.Arg2=Args.Arg3=Args.Arg4=Args.Arg5=Args.Arg6=Args.Arg7=0;
    ArmCallSmc(&Args);
    *(volatile UINT32 *)0x010ac000=0;
    __asm__ volatile("dsb sy":::"memory");
  }
  for (;;) __asm__ volatile("wfe");
}
