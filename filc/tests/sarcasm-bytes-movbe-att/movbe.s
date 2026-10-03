# movbe in its raw perlasm encoding: `.byte 0x0f,0x38,0xf1,<modrm>` is the
# 0F 38 F1 store form (`movbel %eax,(%rdi)`) and `0x0f,0x38,0xf0,<modrm>` the
# load form — exactly what OpenSSL's legacy-gas workaround emits. Decoded and
# byte-swapping like the spelled mnemonic.
	.text
	.globl	movbe_bytes
	.type	movbe_bytes, @function
movbe_bytes:                    ;! long(ptr, long)
	# store the byte-swapped value through the pointer, then load it back
	# plain: the round trip swaps the bytes exactly once.
	movl	%esi, %eax
	.byte	0x0f,0x38,0xf1,0x07      # movbel %eax,(%rdi)
	.byte	0x0f,0x38,0xf0,0x07      # movbel (%rdi),%eax  (swap back)
	movl	(%rdi), %eax              # ... and read the original bytes
	ret
	.size	movbe_bytes, .-movbe_bytes
	.section	.note.GNU-stack,"",@progbits
