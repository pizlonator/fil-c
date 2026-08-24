/* A `.size`-less function body ends at a trailing section switch: the dead,
   unreferenced .rodata string below is TOP-LEVEL content, so it is silently
   dropped per the dead-content rule (exactly as it is when the function does
   carry a .size) — not rejected as "data in a function body". The function must
   compile and return 42. */
	.text
	.global	f
	.type	f, %function
f:                              ;! long(void)
	mov	x0, #42
	ret
	.section	.rodata
.Ldead:
	.asciz	"dead string"
