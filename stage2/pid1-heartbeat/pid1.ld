ENTRY(start)
SECTIONS {
  . = 0x100004000;
  .text : { *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss : { *(.bss*) *(COMMON) }
  /DISCARD/ : { *(.comment*) *(.note*) *(.eh_frame*) }
}
