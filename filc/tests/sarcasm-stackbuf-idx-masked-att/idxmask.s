	.section	.rodata
	.align	64
.Lpat11:
	.quad	17,17,17,17,17,17,17,17
.Lpat22:
	.quad	34,34,34,34,34,34,34,34
	.text
# Gap 3 (indexed + masked compose): `{%kN}`-masked 64-byte vector moves
# through INDEXED stack-buffer operands lower into the redirected buffer
# region with the runtime bounds check on the index preceding the whole
# sequence, the unmasked full-width load, the masked register-register
# merge, and (for stores) the unmasked store-back. Masked-off lanes keep
# their old value; masked-on lanes come from memory (loads) or the source
# (stores). The reads back through the indexed and static buffer spellings
# prove the lane behavior at runtime.
	.globl	idxmask
	.type	idxmask, @function
idxmask:                         ;! long(size_t)
	pushq	%rbx
	subq	$160, %rsp
	movq	%rdi, %rbx              # the byte index into the 128-byte buffer
	vmovdqa64	.Lpat11(%rip), %zmm0
	vmovdqu64	%zmm0, 0(%rsp)          # initialize [0,64) with the 17 pattern
	vmovdqu64	%zmm0, 64(%rsp)         # initialize [64,128)
	movl	$15, %eax
	kmovw	%eax, %k1                  # qword lanes 0-3 masked on (32 bytes)
	# a masked LOAD through the indexed spelling: lanes 0-3 take the buffer's
	# untouched 17s, lanes 4-7 keep the preloaded 34s (merge masking)
	vmovdqa64	.Lpat22(%rip), %zmm2
	vmovdqa64	0(%rsp,%rbx,1), %zmm2{%k1} #! stack buffer (b, %rsp, %rsp + 128)
	vmovq	%xmm2, %rax
	cmpq	$17, %rax
	jne	.bad
	# a masked STORE through the indexed spelling: qword lanes 0-3 write 34s,
	# lanes 4-7 leave the old memory alone
	vmovdqa64	.Lpat22(%rip), %zmm1
	vmovdqa64	%zmm1, 0(%rsp,%rbx,1){%k1} #! stack buffer (b, %rsp, %rsp + 128)
	# the lane before the written span keeps the old pattern
	movq	-8(%rsp,%rbx,1), %rax   #! stack buffer (b)
	cmpq	$17, %rax
	jne	.bad
	# the first written lane holds the new value
	movq	0(%rsp,%rbx,1), %rax    #! stack buffer (b)
	cmpq	$34, %rax
	jne	.bad
	# the first lane after the written span keeps the old pattern
	movq	32(%rsp,%rbx,1), %rax   #! stack buffer (b)
	cmpq	$17, %rax
	jne	.bad
	movl	$1, %eax
	jmp	.done
.bad:
	xorl	%eax, %eax
.done:
	addq	$160, %rsp
	popq	%rbx
	ret
	.size	idxmask, .-idxmask
	.section	.note.GNU-stack,"",@progbits
