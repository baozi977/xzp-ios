/* SPDX-License-Identifier: MIT
 * SO-04J Linux-protocol entry and retained-console probe.
 * No stack, MMU setup, peripheral MMIO, storage or secondary CPU startup.
 * Writes only the measured ramoops console at [0xffcc0000,0xffd00000).
 */
.section .text.boot, "ax"
.balign 8
.global _start
_start:
    b primary_entry
    .long 0
    .quad 0x80000
    .quad image_end - _start
    .quad 0xa
    .quad 0
    .quad 0
    .quad 0
    .ascii "ARMd"
    .long 0

.macro FIELD label, value
    adr x13, \label
    bl append_string
    mov x15, \value
    bl append_hex
    mov w14, #10
    strb w14, [x11], #1
    bl commit_console
.endm

primary_entry:
    mov x19, x0
    mov x20, x1
    mov x21, x2
    mov x22, x3
    mrs x23, CurrentEL
    msr daifset, #0xf
    cmp x23, #4
    b.eq from_el1
    cmp x23, #8
    b.ne reset_system
    mrs x24, sctlr_el2
    b check_mmu
from_el1:
    mrs x24, sctlr_el1
check_mmu:
    /* Do not interpret the physical log address through unknown translation. */
    tbnz x24, #0, reset_system
    /* This first probe requires uncached entry; no cache policy is changed. */
    tbnz x24, #2, reset_system
    movz x17, #0xffcc, lsl #16
    add x11, x17, #12
    adr x13, marker
    bl append_string
    bl commit_console
    FIELD label_el, x23
    FIELD label_sctlr, x24
    FIELD label_x0, x19
    FIELD label_x1, x20
    FIELD label_x2, x21
    FIELD label_x3, x22
    mrs x27, midr_el1
    FIELD label_midr, x27
    mrs x27, mpidr_el1
    FIELD label_mpidr, x27
    mrs x27, id_aa64mmfr0_el1
    FIELD label_mmfr0, x27
    mrs x27, id_aa64mmfr1_el1
    FIELD label_mmfr1, x27
    mrs x27, S3_0_C0_C7_2 /* ID_AA64MMFR2_EL1 */
    FIELD label_mmfr2, x27
    mrs x27, cntfrq_el0
    FIELD label_cntfrq, x27
    adr x13, end_marker
    bl append_string
    bl commit_console
reset_system:
    /* PSCI SYSTEM_RESET, the measured Sony /psci method is SMC. */
    movz x0, #9
    movk x0, #0x8400, lsl #16
    mov x1, xzr
    mov x2, xzr
    mov x3, xzr
    smc #0
halt:
    wfe
    b halt

append_string:
    ldrb w14, [x13], #1
    cbz w14, string_done
    strb w14, [x11], #1
    b append_string
string_done:
    ret

append_hex:
    mov x9, #60
hex_digit:
    lsr x14, x15, x9
    and x14, x14, #15
    cmp x14, #10
    b.lo decimal_digit
    add x14, x14, #87
    b store_digit
decimal_digit:
    add x14, x14, #48
store_digit:
    strb w14, [x11], #1
    subs x9, x9, #4
    b.ge hex_digit
    ret

commit_console:
    sub x16, x11, x17
    sub x16, x16, #12
    dsb sy
    str w16, [x17, #4] /* start */
    str w16, [x17, #8] /* size */
    movz w14, #0x4244
    movk w14, #0x4347, lsl #16
    str w14, [x17]     /* persistent_ram signature, commit last */
    dsb sy
    ret

marker:       .asciz "XZPIOS_STAGE1_20261027_ENTRY_A\n"
label_el:     .asciz "CurrentEL_raw=0x"
label_sctlr:  .asciz "SCTLR_entry=0x"
label_x0:     .asciz "boot_x0_fdt=0x"
label_x1:     .asciz "boot_x1=0x"
label_x2:     .asciz "boot_x2=0x"
label_x3:     .asciz "boot_x3=0x"
label_midr:   .asciz "MIDR_EL1=0x"
label_mpidr:  .asciz "MPIDR_EL1=0x"
label_mmfr0:  .asciz "ID_AA64MMFR0_EL1=0x"
label_mmfr1:  .asciz "ID_AA64MMFR1_EL1=0x"
label_mmfr2:  .asciz "ID_AA64MMFR2_EL1=0x"
label_cntfrq: .asciz "CNTFRQ_EL0=0x"
end_marker:   .asciz "XZPIOS_STAGE1_END_RESET_SMC\n"
.balign 4096, 0
image_end:
