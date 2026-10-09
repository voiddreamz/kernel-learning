ENTRY(_start)
SECTIONS{
    . = 0x900;
    .text : {
        *(.text)
    }
    .rodata : {
        *(.rodata)
    }
    .data :{
        *(.data)
    }
    .bss : {
        *(COMMON)
        *(.bss)
    }
    _kernel_end = .;
}