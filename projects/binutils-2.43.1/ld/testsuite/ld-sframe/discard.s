	.text
	.cfi_sections .sframe
	.globl	foo
	.type	foo, @function
foo:
	.cfi_startproc
	.cfi_def_cfa_offset 16
	.cfi_endproc

	.globl _start
_start:
	.long   foo
