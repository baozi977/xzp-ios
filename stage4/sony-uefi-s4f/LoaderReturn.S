// SPDX-License-Identifier: MIT
// Fixed guarded upstream return path plus a stackless local payload.
#define icc_pmr_el1 S3_0_C4_C6_0
#define icc_igrpen1_el1 S3_0_C12_C12_7
#define icc_bpr1_el1 S3_0_C12_C12_3
#define id_aa64mmfr2_el1 S3_0_C0_C7_2
.text
.balign 64
.globl loader_save_ctx
loader_save_ctx:
  adrp x10, loader_ctx
  add  x10, x10, :lo12:loader_ctx
  mrs x9, ttbr0_el1
   str x9, [x10, #0]
  mrs x9, tcr_el1
     str x9, [x10, #8]
  mrs x9, mair_el1
    str x9, [x10, #16]
  mrs x9, sctlr_el1
   str x9, [x10, #24]
  mrs x9, vbar_el1
    str x9, [x10, #32]
  mrs x9, cpacr_el1
   str x9, [x10, #40]
  mov x9, sp
          str x9, [x10, #48]
  mrs x9, spsel
       str x9, [x10, #56]
  mrs x9, daif
        str x9, [x10, #64]
  mrs x9, cntv_ctl_el0
  str x9, [x10, #72]
  mrs x9, cntv_cval_el0
 str x9, [x10, #80]
  mrs x9, cntkctl_el1
   str x9, [x10, #104]
  mrs x9, tpidr_el1
     str x9, [x10, #280]
  mrs x9, tpidr_el0
     str x9, [x10, #288]
  mrs x9, tpidrro_el0
   str x9, [x10, #296]
  mrs x9, contextidr_el1
 str x9, [x10, #312]
  mrs x9, mdscr_el1
     str x9, [x10, #320]
  mrs x9, s3_3_c4_c4_0
          str x9, [x10, #328]
  mrs x9, s3_3_c4_c4_1
          str x9, [x10, #336]
  mrs x9, icc_pmr_el1
     str x9, [x10, #344]
  mrs x9, icc_igrpen1_el1
 str x9, [x10, #352]
  mrs x9, icc_bpr1_el1
    str x9, [x10, #360]
  mrs x9, spsel
  cbz x9, 1f
  mrs x9, sp_el0
        str x9, [x10, #304]
1:
  str x18, [x10, #112]
  stp x19, x20, [x10, #120]
  stp x21, x22, [x10, #136]
  stp x23, x24, [x10, #152]
  stp x25, x26, [x10, #168]
  stp x27, x28, [x10, #184]
  stp x29, x30, [x10, #200]
  mov x0, #0
  ret
.balign 64
.globl loader_return_stub
loader_return_stub:
  msr daifset, #0xf
  mrs x9, id_aa64mmfr1_el1
  ubfx x9, x9, #20, #4
  cbz x9, 7f
  .inst 0xd500409f
7:
  mrs x9, id_aa64mmfr2_el1
  ubfx x9, x9, #4, #4
  cbz x9, 8f
  .inst 0xd500407f
8:
  adrp x10, KernelEntryState
  add x10, x10, :lo12:KernelEntryState
  mrs x9, sctlr_el1
  str x9, [x10]
  mrs x9, tcr_el1
  str x9, [x10,#8]
  mrs x9, ttbr0_el1
  str x9, [x10,#16]
  mrs x9, ttbr1_el1
  str x9, [x10,#24]
  mrs x9, mair_el1
  str x9, [x10,#32]
  mrs x9, id_aa64mmfr0_el1
  str x9, [x10,#40]
  dc cvac,x10
  add x9,x10,#40
  dc cvac,x9
  dsb sy
  mrs x9, sctlr_el1
  bic x9, x9, #0x1
  bic x9, x9, #0x4
  bic x9, x9, #0x1000
  msr sctlr_el1, x9
  isb
  adrp x10, loader_ctx
  add  x10, x10, :lo12:loader_ctx
  ldr x9, [x10, #0]
   msr ttbr0_el1, x9
  ldr x9, [x10, #8]
   msr tcr_el1, x9
  ldr x9, [x10, #16]
  msr mair_el1, x9
  ldr x9, [x10, #32]
  msr vbar_el1, x9
  ldr x9, [x10, #40]
  msr cpacr_el1, x9
  isb
  tlbi vmalle1
  dsb sy
  ic iallu
  isb
  ldr x9, [x10, #24]
  msr sctlr_el1, x9
  isb
  ldr x9, [x10, #56]
  cbz x9, 1f
  msr spsel, #1
  ldr x9, [x10, #304]
 msr sp_el0, x9
  b 2f
1:
  msr spsel, #0
2:
  ldr x9, [x10, #48]
  mov sp, x9
  ldr x9, [x10, #280]
 msr tpidr_el1, x9
  ldr x9, [x10, #288]
 msr tpidr_el0, x9
  ldr x9, [x10, #296]
 msr tpidrro_el0, x9
  ldr x9, [x10, #312]
 msr contextidr_el1, x9
  ldr x9, [x10, #320]
 msr mdscr_el1, x9
  ldr x9, [x10, #344]
 msr icc_pmr_el1, x9
  ldr x9, [x10, #360]
 msr icc_bpr1_el1, x9
  ldr x9, [x10, #352]
 msr icc_igrpen1_el1, x9
  ldr x9, [x10, #328]
 msr s3_3_c4_c4_0, x9
  ldr x9, [x10, #336]
 msr s3_3_c4_c4_1, x9
  ldr x9, [x10, #104]
 msr cntkctl_el1, x9
  ldr x9, [x10, #80]
  msr cntv_cval_el0, x9
  ldr x9, [x10, #72]
  msr cntv_ctl_el0, x9
  ldr x18, [x10, #112]
  ldp x19, x20, [x10, #120]
  ldp x21, x22, [x10, #136]
  ldp x23, x24, [x10, #152]
  ldp x25, x26, [x10, #168]
  ldp x27, x28, [x10, #184]
  ldp x29, x30, [x10, #200]
  stp x0, x1, [x10, #216]
  stp x2, x3, [x10, #232]
  stp x4, x5, [x10, #248]
  stp x6, x7, [x10, #264]
  ldr x9, [x10, #64]
  msr daif, x9
  mov x0, #1
  ret

.balign 64
.global resident_payload_stub
resident_payload_stub:
  adr x0, resident_payload_marker
  mov x1, #resident_payload_marker_end-resident_payload_marker
  mov x2, #0
  mrs x3, sctlr_el1
  mrs x4, tcr_el1
  mrs x5, CurrentEL
  mrs x6, S3_0_C0_C7_2
  mrs x7, cntfrq_el0
  b loader_return_stub
resident_payload_marker:
  .ascii "XZPIOS_RESIDENT_PAYLOAD_S4F"
resident_payload_marker_end:
.section .note.GNU-stack,"",%progbits
