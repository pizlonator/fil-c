/* paciasp (and every pacia/pacib/... pointer-auth form) implicitly
   read-modify-writes x30 with a cryptographically authenticated value that
   the register model cannot represent: it fell into the unknown-mnemonic
   fallback with ZERO register effects, silently disconnecting x30's web.
   clang emits paciasp/autiasp under -mbranch-protection, so this is rejected
   cleanly rather than miscompiled. */
	.text
	.global	f
	.type	f, %function
f:                              ;! long(long)
	paciasp
	mov	x0, x1
	autiasp
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
