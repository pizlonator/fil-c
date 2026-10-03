# UNSOUND: returning the D9-promoted frame's region pointer DIRECTLY. The
# escape cluster (`leaq 16(%rsp), %rdi` + `call fill32 ;! void(ptr)`) promotes
# the whole fixed frame to a GC region, and the post-call `leaq 16(%rsp), %rax`
# re-derives a pointer into that region. But %rax still tracks as holding the
# saved stack pointer at the `ret` — the return value would be the live stack
# address, which the rewrite cannot prove safe — so sarcasm rejects this at
# compile time:
#   sarcasm: returning while %rax holds the saved stack pointer cannot be
#   proven safe (the return value would be the live stack address; redefine
#   %rax or recover %rsp from it first): ret
# The accepted spelling launders the pointer through a frame slot with
# `#! store ptr` / `#! load ptr` first (see
# sarcasm-frame-escape-return-att): the load redefines %rax with a plain
# pointer web, which is what makes the return provable.
	.text
	.globl	rejectret
	.type	rejectret, @function
rejectret:                      #! ptr(long)
	pushq	%rbx
	subq	$64, %rsp           # plain fixed frame [0,64): D0 = 72, region [0,64)
	leaq	16(%rsp), %rdi      # region+16 — the ESCAPING lea: promotion
	movq	%rdi, %rsi          # copy of the frame pointer
	leaq	8(%rsi), %rdx       # offset of the copy: region+24
	movq	$4111, (%rdi)       # slot16 = 4111 through the derived pointer
	movq	$4222, (%rdx)       # slot24 = 4222
	call	fill32 ;! void(ptr) # helper writes 100..103 at frame+16..frame+47
	leaq	16(%rsp), %rax      # the region pointer, straight from the frame lea
	addq	$64, %rsp
	popq	%rbx
	ret                          # rejected: %rax holds the saved stack pointer
	.size	rejectret, .-rejectret
	.section	.note.GNU-stack,"",@progbits
