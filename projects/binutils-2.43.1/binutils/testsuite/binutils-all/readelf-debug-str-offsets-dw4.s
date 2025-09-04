	.section	.debug_str.dwo,"MS",%progbits,1
	.asciz	"FIRST"
	.asciz	"SECOND"
	.section	.debug_str_offsets.dwo,"MS",%progbits,1
	.4byte 0
	.4byte 6
	.section        .debug_macro.dwo,"e",%progbits
	.2byte  0x4     /* DWARF macro version number.  */
	.byte   0x0     /* Flags: 32-bit dwarf.  */
	.byte   0xb     /* Define macro strx.  */
	.uleb128 0      /* At line number 0.  */
	.uleb128 0x0    /* .debug_str_offsets.dwo entry 0.  */
	.byte   0       /* End compilation unit.  */
