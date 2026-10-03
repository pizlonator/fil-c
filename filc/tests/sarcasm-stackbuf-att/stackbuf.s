	.file	"stackbuf.c"
	.text
# `#! stack buffer (...)`: annotated STACK BUFFERS with bounds checks.
#
# A long-form declaration `#! stack buffer (id, %reg + lo, %reg + hi)` on an
# indexed stack access declares the frame byte range [rsp+lo, rsp+hi) as a raw
# byte buffer: the access is lowered into a dedicated region of sarcasm's
# synthesized frame with a runtime bounds check on the index, and every static
# (non-indexed) access inside the range is redirected into the same region
# (same bytes, no slot webs, no capabilities). Short forms `#! stack buffer
# (id)` reference a buffer declared by a long form anywhere in the file.

# The exact shape from the feature request: a static store at 16(%rsp) plus an
# indexed load `movl (%rsp,%rdi),%eax` annotated with the buffer. If %rdi is
# 16, the load reads back the stored value (buffer offset 16).
	.globl	sb_basic
	.type	sb_basic, @function
sb_basic:                       ;! int(size_t)
	subq	$64, %rsp
	movl	$0x41424344, 16(%rsp)
	xorl	%ebx, %ebx
	movl	(%rsp,%rdi), %eax #! stack buffer (x, %rsp, %rsp + 32)
	addq	$64, %rsp
	ret
	.size	sb_basic, .-sb_basic

# Write through the buffer with the indexed form, read back through the
# ordinary static spelling: both hit the same bytes of the lowered region.
	.globl	sb_idx_store_static_load
	.type	sb_idx_store_static_load, @function
sb_idx_store_static_load:       ;! int(size_t)
	subq	$64, %rsp
	movl	%edi, (%rsp,%rdi) #! stack buffer (w, %rsp, %rsp + 32)
	movl	4(%rsp), %eax
	addq	$64, %rsp
	ret
	.size	sb_idx_store_static_load, .-sb_idx_store_static_load

# A SIMD store into the buffer and a GPR indexed load of the same bytes: the
# FP cluster machinery must leave buffer ranges alone, and both views must
# agree byte for byte.
	.globl	sb_fp_gpr
	.type	sb_fp_gpr, @function
sb_fp_gpr:                      ;! int(ptr, size_t)
	subq	$64, %rsp
	movdqu	(%rdi), %xmm0
	movdqu	%xmm0, (%rsp)
	movl	(%rsp,%rsi), %eax #! stack buffer (v, %rsp, %rsp + 32)
	addq	$64, %rsp
	ret
	.size	sb_fp_gpr, .-sb_fp_gpr

# Scale-4 indexed form: `movl (%rsp,%rdi,4), %eax`. The bounds check must be
# in units of the index (idx_max = (64 - 4)/4 - 1 = 15), not bytes.
	.globl	sb_scale4
	.type	sb_scale4, @function
sb_scale4:                      ;! int(size_t)
	subq	$64, %rsp
	movl	$0x11111111, 0(%rsp)
	movl	$0x22222222, 4(%rsp)
	movl	$0x33333333, 8(%rsp)
	movl	$0x44444444, 12(%rsp)
	movl	$0x55555555, 60(%rsp)
	movl	(%rsp,%rdi,4), %eax #! stack buffer (s, %rsp, %rsp + 64)
	addq	$64, %rsp
	ret
	.size	sb_scale4, .-sb_scale4

# Scale-8 indexed form: a qword access, idx in [0, 7].
	.globl	sb_scale8
	.type	sb_scale8, @function
sb_scale8:                      ;! int(size_t)
	subq	$64, %rsp
	movabsq	$0x0102030405060708, %r11
	movq	%r11, 56(%rsp)
	movq	(%rsp,%rdi,8), %rax #! stack buffer (s8, %rsp, %rsp + 64)
	addq	$64, %rsp
	ret
	.size	sb_scale8, .-sb_scale8

# Two overlapping buffers declared in one function: they merge into one larger
# lowered region, and a store through o1's range is readable through o2's
# range at the same bytes.
	.globl	sb_overlap
	.type	sb_overlap, @function
sb_overlap:                     ;! int(size_t)
	subq	$64, %rsp
	movl	$0xcafebabe, 16(%rsp)
	movl	$0x0badf00d, 20(%rsp)
	movl	(%rsp,%rdi), %eax #! stack buffer (o1, %rsp, %rsp + 32)
	movl	20(%rsp), %ecx #! stack buffer (o2, %rsp + 16, %rsp + 48)
	xorl	%ecx, %eax
	addq	$64, %rsp
	ret
	.size	sb_overlap, .-sb_overlap

# Short form in a second function: the long form lives in sb_basic (same
# file); this function's short-form access must independently bounds-check
# against its own frame.
	.globl	sb_short_form
	.type	sb_short_form, @function
sb_short_form:                  ;! int(size_t)
	subq	$48, %rsp
	movl	$0x56575859, 12(%rsp)
	movl	(%rsp,%rdi), %eax #! stack buffer (x)
	addq	$48, %rsp
	ret
	.size	sb_short_form, .-sb_short_form

# Re-entrancy: each recursive call's buffer is a different region of the real
# stack, so a deeper call must not clobber the caller's buffer bytes.
	.globl	sb_rec
	.type	sb_rec, @function
sb_rec:                         ;! int(size_t)
	subq	$48, %rsp
	movl	%edi, (%rsp) #! stack buffer (r, %rsp, %rsp + 16)
	testl	%edi, %edi
	je	.Lsb_rec_base
	subl	$1, %edi
	callq	sb_rec ;! int(size_t)
.Lsb_rec_base:
	movl	(%rsp), %eax #! stack buffer (r)
	addq	$48, %rsp
	ret
	.size	sb_rec, .-sb_rec
	.section	.note.GNU-stack,"",@progbits
