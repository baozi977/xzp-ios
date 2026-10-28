ENTRY(_start)
SECTIONS {
    . = 0;
    .text : { KEEP(*(.text.boot)) }
    /DISCARD/ : { *(.note*) *(.comment*) }
}
