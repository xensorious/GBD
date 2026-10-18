ENTRY(_start)
MEMORY {
    ROM (rx) : ORIGIN = 0x08000000, LENGTH = 32M
    EWRAM (rwx) : ORIGIN = 0x02000000, LENGTH = 256K
    IWRAM (rwx) : ORIGIN = 0x03000000, LENGTH = 32K
}
SECTIONS {
    .text : { KEEP(*(.gba.header)) KEEP(*(.rodata.save_type)) *(.text*) *(.rodata*) } > ROM
    .data : { __data_start = .; *(.data*) __data_end = .; } > EWRAM AT> ROM
    __data_load = LOADADDR(.data);
    .bss (NOLOAD) : { __bss_start = .; *(.bss*) *(COMMON) __bss_end = .; } > EWRAM
    /DISCARD/ : { *(.comment) *(.note*) *(.ARM.exidx*) *(.ARM.extab*) }
}
