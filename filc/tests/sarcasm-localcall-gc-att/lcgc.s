# GC pressure with pointers live across local calls: the caller loops calling
# a local subroutine while the pointer array (%r13), the count/frame state,
# and the running sum (%rbx) stay live. The subroutine loads pointers through
# the caller's registers (+8-rule frame reads for the loop bound), chases
# them with `;! load ptr`, and performs a `.alloca` (a real GC allocation)
# on every call. If the clone's roots, spills, or the in-clone allocation
# corrupt the caller's live pointers, the sum mismatches.
	.text
	.globl	lc_gc_sum
	.type	lc_gc_sum, @function
lc_gc_sum:                      ;! long(ptr,long)
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	subq	$16, %rsp
	movq	%rdi, %r13
	movq	%rsi, 0(%rsp)
	movq	$0, %r12
	xorq	%rbx, %rbx
.Lgc_loop:
	movq	0(%rsp), %rax
	cmpq	%rax, %r12
	jge	.Lgc_done
	call	subwork
	incq	%r12
	jmp	.Lgc_loop
.Lgc_done:
	movq	%rbx, %rax
	addq	$16, %rsp
	popq	%r13
	popq	%r12
	popq	%rbx
	ret
	.size	lc_gc_sum, .-lc_gc_sum
	.type	subwork, @function
subwork:
	.alloca	$64, $16, %fil_buf
	movq	(%r13,%r12,8), %r11  ;! load ptr
	movzbq	(%r11), %r10
	movzbq	1(%r11), %rax
	addq	%rax, %r10
	movzbq	2(%r11), %rax
	addq	%rax, %r10
	movzbq	3(%r11), %rax
	addq	%rax, %r10
	movq	%r10, 0(%fil_buf)
	movq	0(%fil_buf), %r10
	addq	%r10, %rbx
	ret
	.size	subwork, .-subwork
	.section	.note.GNU-stack,"",@progbits
