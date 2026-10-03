	.file	"innerframe.c"
	.text
# `#! stack buffer (...)` in a MID-FUNCTION INNER FRAME (the aesni CBC
# decrypt shape): the prologue scan ends in the leading half of the function
# (here: the `call` ends the scan), so the second half's push+sub keys as a
# mid-function inner frame and its bytes normalize to NEGATIVE frame offsets
# (D0 - d below the frame base). A buffer declared at that depth resolves to
# [D0 - d, D0 - d + N) — the function's own provably-allocated stack — and is
# legal as long as it stays at or above the declaring statement's rsp and
# clear of the outstanding push slots. The static temp store and the
# rep movsb source alias must lower into the SAME region bytes, and the
# indexed form's runtime bounds check must cover the lowered range.

# if_ext: a two-half function; the second half allocates a 48-byte inner
# frame and runs the aesni CBC tail shape: store the 16-byte temp to the
# frame bottom, lea (%rsp) as the rep source, copy `16 - n` bytes to the
# heap destination. With n = 16 the copy is empty.
	.globl	if_ext
	.type	if_ext, @function
if_ext:                             ;! void(ptr, size_t)
	testl	%esi, %esi
	jz	.Lif_ret
# --- leading half (owns the prologue scan; no frame traffic) ---
	call	if_helper ;! void()
# --- second half: mid-function inner frame ---
	pushq	%rbp
	subq	$48, %rsp
	andq	$-16, %rsp
	movl	$0x03020100, (%rsp)    # the 16-byte temp: byte i == i,
	movl	$0x07060504, 4(%rsp)   # normalized [-56, -40)
	movl	$0x0b0a0908, 8(%rsp)
	movl	$0x0f0e0d0c, 12(%rsp)
	movq	$16, %rcx
	movq	%rdi, %rdx
	subq	%rsi, %rcx             # rcx = 16 - n  (0 .. 15)
	leaq	(%rsp), %rsi           # rep source: stack alias at the frame bottom
	rep	movsb                  #! stack buffer (cbcdec, %rsp, %rsp + 16)
	movq	$0, (%rsp)             # scrub the temp
	movq	$0, 8(%rsp)
	addq	$48, %rsp
	popq	%rbp
.Lif_ret:
	ret
	.size	if_ext, .-if_ext

	.globl	if_helper
	.type	if_helper, @function
if_helper:                          ;! void()
	ret
	.size	if_helper, .-if_helper

# Indexed access inside the same inner-frame shape: the annotated load's
# bounds check must cover [0,16) of the lowered inner-frame buffer.
	.globl	if_idx
	.type	if_idx, @function
if_idx:                             ;! long(size_t)
	call	if_helper ;! void()
	pushq	%rbp
	subq	$48, %rsp
	andq	$-16, %rsp
	movl	$0x45464748, (%rsp)
	movl	$0x41424344, 4(%rsp)
	movq	$0, 8(%rsp)
	xorl	%eax, %eax
	movq	(%rsp,%rdi), %rax #! stack buffer (idxbuf, %rsp, %rsp + 16)
	addq	$48, %rsp
	popq	%rbp
	ret
	.size	if_idx, .-if_idx

	.section	.note.GNU-stack,"",@progbits
