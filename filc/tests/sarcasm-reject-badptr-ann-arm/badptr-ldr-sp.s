/* An sp-relative access is a virtualized frame slot, not a heap access: there
   is no capability to load a pointer through, so `;! load ptr` on [sp] is
   rejected at the annotation site. */
	.text
	.global	f
	.type	f, %function
f:                              ;! long(ptr)
	sub	sp, sp, #32
	ldr	x0, [sp]        ;! load ptr
	add	sp, sp, #32
	ret
	.size	f, .-f
