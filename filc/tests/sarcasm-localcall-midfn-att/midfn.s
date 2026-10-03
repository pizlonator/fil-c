# A call to a label in the middle of a signatured function's OWN body (the
# OpenSSL aesni set_encrypt_key shape: `call .Lkey_expansion_128` self-calls).
# The mid-body label resolves to a local subroutine whose region is a pristine
# copy of the owner's whole body entered at that label; each caller gets its
# own renamed clone of the reachable code, and the owner's own callsites are
# no exception (the caller's frame IS the owner's frame). Mechanism:
# - localcall.discover builds a mid-label -> owner map over the sig'd function
#   bodies (localcall.luau) and claims the target through the same fnRegion
#   machinery as an entry-adjacent alias, with entry = the mid-label and the
#   clone range = the reachable set from it.
# - The clone runs on the caller's stack at the caller's rsp; this region
#   touches no frame slots (registers and a lea only), so it is sound in any
#   caller — and here the caller is the owner itself.
# Contrast sarcasm-localcall-midlabel-att (mid-label of a top-level
# SUBROUTINE region) and sarcasm-reject-localcall-midbody-frame-cross (a
# cross-function call to a mid-body label whose region addresses the owner's
# frame — rejected).
	.text
	.globl	outer_midfn
	.type	outer_midfn, @function
outer_midfn:                    ;! long(long)
	endbr64
	movq	%rdi, %r10
	call	.Lmidsub
	movq	%r9, %rax
	ret
	.align	16
.Lmidsub:
	leaq	(%r10,%r10), %r9
	addq	$3, %r9
	ret
	.size	outer_midfn, .-outer_midfn
	.section	.note.GNU-stack,"",@progbits
