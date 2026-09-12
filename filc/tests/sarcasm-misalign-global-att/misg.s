	.text
	# Misaligned loads need only hardware-required alignment (x86 tolerates
	# them), exactly like Fil-C compiled code: an 8-byte load at a 4-byte
	# offset inside a global object succeeds — same-file and extern alike.
	# (The old aesni-sha1-x86_64.pl workaround recomposed
	# `OPENSSL_ia32cap_P+4(%rip)` from two 32-bit loads; that SARCASM-gated
	# special case is unnecessary.)
	.globl	cap_load_mis
	.type	cap_load_mis, @function
cap_load_mis:                   ;! long()
	endbr64
	movq	OPENSSL_ia32cap_P+4(%rip), %rax
	ret
	.size	cap_load_mis, .-cap_load_mis
	.globl	extcap_load_mis
	.type	extcap_load_mis, @function
extcap_load_mis:                ;! long()
	endbr64
	movq	extcap+4(%rip), %rax #! global ptr
	ret
	.size	extcap_load_mis, .-extcap_load_mis
	.globl	cap_store_mis
	.type	cap_store_mis, @function
cap_store_mis:                  ;! void(long)
	endbr64
	movq	%rdi, OPENSSL_ia32cap_P+4(%rip)
	ret
	.size	cap_store_mis, .-cap_store_mis
	.globl	extcap_store_mis
	.type	extcap_store_mis, @function
extcap_store_mis:               ;! void(long)
	endbr64
	movq	%rdi, extcap+4(%rip) #! global ptr
	ret
	.size	extcap_store_mis, .-extcap_store_mis
	.data
	.p2align	2
OPENSSL_ia32cap_P:
	.long	0x11111111
	.long	0x22222222
	.long	0x33333333
	.section	.note.GNU-stack,"",@progbits
