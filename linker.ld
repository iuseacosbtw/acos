ENTRY(_start)

SECTIONS
{
    . = 0x7E00;

    .text :
    {
        *(.text)
    }

    .rodata :
    {
        *(.rodata*)
    }

	. = 0x100000;

    .data :
    {
        *(.data)
    }

	_linker_bss_start = .;
    .bss :
    {
        *(COMMON)
        *(.bss*)
    }
	_linker_bss_end = .;
}
