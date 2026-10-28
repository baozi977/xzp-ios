#
#  Copyright (c) 2018, Linaro Limited. All rights reserved.
#
#  This program and the accompanying materials
#  are licensed and made available under the terms and conditions of the BSD License
#  which accompanies this distribution.  The full text of the license may be found at
#  http://opensource.org/licenses/bsd-license.php
#
#  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
#  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#
#

#include <AsmMacroIoLibV8.h>
#include <Library/ArmLib.h>

.text
.align 3

ASM_FUNC(ArmPlatformPeiBootAction)
startlabel:
  msr daifset, #0xf
  // check if we're located at expected location
  adr	x4, ASM_PFX(ArmPlatformPeiBootAction)
  ldr	x5, =ArmPlatformPeiBootAction
  cmp	x4, x5
  bne	docopy
  ret
docopy:
  MOV64 (x10, 0xffb00000)
  str x0, [x10] // Sony boot FDT, captured before relocation
  MOV64 (x9, 0xffcc0000)
  str wzr, [x9]
  dsb sy
  // find our start address by getting our expected offset, then subtracting it from our actual address
  ldr	x6, =FixedPcdGet64 (PcdFdBaseAddress)
  sub	x5, x5, x6 // x5 now holds offset of ArmPlatformPeiBootAction from start of FD base
  sub	x4, x4, x5 // x4 now holds address of actual FD base
  // tweak the return address
  // note: x30 is lr; gcc5 doesn't have the alias
  sub	x30, x30, x4
  add	x30, x30, x6
  ldr	x5, =FixedPcdGet64 (PcdFdSize)
  // crap memcpy
loop:
  ldp	x2, x3, [x4], #16
  stp	x2, x3, [x6], #16
  subs	x5, x5, #16
  b.ne	loop
  // x4 now points just past the relocated FD in the original Image.
  // Copy the appended, fixed kernel to its isolated staging reservation.
  MOV64 (x6, 0xf0000000)
  MOV64 (x5, 0xf92a70)
kernel_copy:
  ldp x2, x3, [x4], #16
  stp x2, x3, [x6], #16
  subs x5, x5, #16
  b.ne kernel_copy
  dsb sy
  ret
.ltorg

//UINTN
//ArmPlatformIsPrimaryCore (
//  IN UINTN MpId
//  );
ASM_FUNC(ArmPlatformIsPrimaryCore)
  MOV32 (w1, FixedPcdGet32(PcdArmPrimaryCoreMask))
  and   x0, x0, x1
  MOV32 (w1, FixedPcdGet32(PcdArmPrimaryCore))
  cmp   w0, w1
  cset  x0, eq
  ret

//UINTN
//ArmPlatformGetPrimaryCoreMpId (
//  VOID
//  );
ASM_FUNC(ArmPlatformGetPrimaryCoreMpId)
  MOV32 (w0, FixedPcdGet32(PcdArmPrimaryCore))
  ret

//UINTN
//ArmPlatformGetCorePosition (
//  IN UINTN MpId
//  );
// With this function: CorePos = (ClusterId * 4) + CoreId
ASM_FUNC(ArmPlatformGetCorePosition)
  and   x1, x0, #ARM_CORE_MASK
  and   x0, x0, #ARM_CLUSTER_MASK
  add   x0, x1, x0, LSR #6
  ret

