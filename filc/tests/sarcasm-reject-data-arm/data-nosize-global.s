/* A `.size`-less function body ends at the trailing section switch, so the
   GLOBAL data label below is top-level content: it must be rejected with the
   top-level "globals cannot be made memory-safe automatically" message (label
   outside any function), NOT "data in a function body". */
	.text
	.global	f
	.type	f, %function
f:                              ;! long(void)
	mov	x0, #42
	ret
	.section	.rodata
	.global	mystr
mystr:
	.asciz	"hello"
