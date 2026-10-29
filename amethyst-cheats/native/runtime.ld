ENTRY(read_keys_entry)
SECTIONS {
  . = 0x09FF0000;
  .text : { *(.text*) *(.rodata*) }
  ASSERT(. < 0x09FF1000, "runtime exceeds reserved ROM region")
  . = 0x09FF1000;
  .readkeys : { KEEP(*(.readkeys)) }
  . = 0x09FF1010;
  .heapentry : { KEEP(*(.heapentry)) }
  . = 0x09FF1020;
  .collision : { KEEP(*(.collision)) }
  . = 0x09FF1060;
  .guards : { KEEP(*(.guards)) }
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) }
}
