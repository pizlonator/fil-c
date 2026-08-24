/* A global label in a data section names bytes that cannot be given a
   capability automatically: rejected (globals cannot be made memory-safe). */
	.text
	.global	f
	.type	f, %function
f:                              ;! unsigned(void)
	mov	x0, #42
	ret
	.size	f, .-f
	.section	.rodata
	.global	mystr
mystr:
	.ascii	"hello world"
