SECTIONS
{
  . = SEGMENT_START("text-segment", 0) + SIZEOF_HEADERS;
  .rela.dyn : { *(.rela.*) }
  .text : { *(.text) }
  . = DATA_SEGMENT_ALIGN (CONSTANT (MAXPAGESIZE), CONSTANT (COMMONPAGESIZE));
  .data : { *(.data) }
  /DISCARD/ : { *(.discard) }
}
