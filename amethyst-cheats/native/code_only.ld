ENTRY(read_keys_entry)
SECTIONS {
  /* The installer builds a 256-word ROM-derived table at 0200B960.
   * Group the movable code at the heap's upper end so dex sprite buffers
   * can use the contiguous gap between ReadKeys and the action table.
   */
  . = 0x0200CECC;
  .readkeys : { KEEP(*(.readkeys)) }
  . = 0x02010A38;
  .heapentry : { KEEP(*(.heapentry)) }
  ASSERT(. <= 0x02010A48, "ARM heap entry exceeds its reservation")
  . = 0x0201AE60;
  .text : { *(.text*) *(.rodata*) }
  ASSERT(. <= 0x0201BD10, "code-only runtime overlaps entry mode")
  . = 0x0201BD10;
  .entrymode : { KEEP(*(.entrymode)) }
  ASSERT(. <= 0x0201BE60, "entry mode overlaps battle callback clone")
  . = 0x0201BFA0;
  .guards : { KEEP(*(.guards)) }
  . = 0x0201BFC0;
  .callbacks : { KEEP(*(.callbacks)) }
  ASSERT(. <= 0x0201BFE0, "guard export exceeds its reserved interval")
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) }
}
