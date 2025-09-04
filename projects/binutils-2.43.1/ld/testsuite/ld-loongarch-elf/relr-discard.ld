OUTPUT_ARCH(loongarch64)
ENTRY(_start)
SECTIONS
{
  /DISCARD/ : { *(.discard.*) }

  . = 0x10000;
  .text : { *(.text) }
  . = 0x20000;
  .got  : { *(.got) *(.got.plt)}
  . = 0x30000;
  .data : { *(.data) *(.data.*) }
}
