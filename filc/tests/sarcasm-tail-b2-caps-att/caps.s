# B2 mid-body join with pointer capabilities live in %r10/%r11 (the chacha
# shape: the jumper's capability words reach the clone unchanged — NO
# marshalling happens on a B2 join, exactly like a hardware `jmp`). The
# jumper moves its ptr arguments into %r10/%r11 and jumps into its sibling's
# mid-body tail; the clone dereferences both. A B1 tail call would marshal
# and drop the %r10/%r11 words, so executing this through the join proves the
# B2 reading (and its register-state sharing) is what ran.
	.text
	.globl	caps_sum
	.type	caps_sum, @function
caps_sum:                       ;! long(ptr,ptr,long)
	movq	%rdi, %r10
	movq	%rsi, %r11
	jmp	.Lsum_tail
	.size	caps_sum, .-caps_sum
	.globl	caps_entry
	.type	caps_entry, @function
caps_entry:                     ;! long(ptr,ptr,long)
	movq	%rdi, %r10
	movq	%rsi, %r11
	nop
.Lsum_tail:
	# shared tail: r10[0] + r11[0] + rdx
	movq	(%r10), %rax
	addq	(%r11), %rax
	addq	%rdx, %rax
	ret
	.size	caps_entry, .-caps_entry
	.section	.note.GNU-stack,"",@progbits
