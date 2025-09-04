/* Script for DT_RELR tests */
OUTPUT_ARCH(loongarch64)
ENTRY(_start)
SECTIONS
{
	PROVIDE (__executable_start = 0x8000); . = 0x10000;
	.text :
	{
		*(.before)
		*(.text)
		*(.after)
	} =0
	. = 0x20000;
	.got :
	{
		*(.got)
		*(.got.plt)
	}
	. = 0x12340000;
	.data :
	{
		*(.data)
	}
}
