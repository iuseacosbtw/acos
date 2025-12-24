OUTPUT_FORMAT("binary")
ENTRY(_start)

SECTIONS
{
    . = 0x0000;
    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) }
}
