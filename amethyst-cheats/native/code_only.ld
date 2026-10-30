ENTRY(read_keys_entry)
SECTIONS {
  /* The installer builds a 256-word ROM-derived table at 0200B960. */
  . = 0x0200CECC;
  .readkeys : { KEEP(*(.readkeys)) }
  . = 0x0200D100;
  .text : { *(.text*) *(.rodata*) }
  ASSERT(. < 0x0200E000, "code-only runtime overlaps ExecuteActions clone")
  . = 0x02010A38;
  .heapentry : { KEEP(*(.heapentry)) }
  ASSERT(. <= 0x02010B00, "ARM heap entry overlaps guard export")
  . = 0x02010B00;
  .guards : { KEEP(*(.guards)) }
  . = 0x02010B20;
  .callbacks : { KEEP(*(.callbacks)) }
  ASSERT(. <= 0x02010C00, "guard export overlaps runtime state")
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) }
}
