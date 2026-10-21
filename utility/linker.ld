ENTRY(main)

SECTIONS
{
    . = 0;

    .text : {
        *(.text.main)
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(COMMON)
        *(.bss)
    }
}
