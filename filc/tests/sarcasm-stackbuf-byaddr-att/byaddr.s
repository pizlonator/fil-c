	.file "byaddr.c"
	.text
# Feature A: by-ADDRESS `#! stack buffer` accesses. A register may hold the
# ADDRESS of a declared stack buffer's bytes (materialized by a buffer-interior
# `leaq`), be computed on (add/sub/xor), stored, reloaded, or cmov'd, and any
# access through it annotated `#! stack buffer (id)` lowers to a RUNTIME BOUNDS
# CHECK of the address against the buffer's lowered range followed by the RAW
# instruction (no capability check — the buffer address is a capability-less
# stack address, which the ordinary checked path would trap on).

# The base case: `leaq 8(%rsp), %rax` computes the address of buffer byte 8;
# `movl (%rax)` and `movl 4(%rax)` access bytes 8..15 by value. The static
# spill at 8(%rsp) seeds the buffer bytes so the reads are deterministic.
	.globl	ba_basic
	.type	ba_basic, @function
ba_basic:                       ;! long(size_t)
	subq	$64, %rsp
	movabsq	$0x1122334455667788, %r10
	movq	%r10, 8(%rsp) #! stack buffer (x, %rsp, %rsp + 32)
	leaq	8(%rsp), %rbx
	movl	(%rbx), %eax #! stack buffer (x)
	movl	%eax, %r11d
	addq	$4, %rbx
	movl	(%rbx), %eax #! stack buffer (x)
	shlq	$32, %rax
	orq	%r11, %rax
	addq	$64, %rsp
	ret
	.size	ba_basic, .-ba_basic

# Sub on the address (walking DOWN into the buffer) and an xor on the VALUE:
# the address arithmetic is ordinary integer arithmetic.
	.globl	ba_alu
	.type	ba_alu, @function
ba_alu:                         ;! long(size_t)
	subq	$64, %rsp
	movabsq	$0xfeedfacefeedface, %r10
	movq	%r10, 16(%rsp) #! stack buffer (y, %rsp, %rsp + 48)
	movq	$0x0102030405060708, %r10
	movq	%r10, 8(%rsp) #! stack buffer (y)
	leaq	24(%rsp), %rbx
	addq	$-8, %rbx
	movl	(%rbx), %eax #! stack buffer (y)
	subq	$8, %rbx
	movl	(%rbx), %r11d #! stack buffer (y)
	shlq	$32, %rax
	orq	%r11, %rax
	addq	$64, %rsp
	ret
	.size	ba_alu, .-ba_alu

# The buffer address STORED to a frame slot and RELOADED: plain integer moves;
# the reloaded web drives the same by-address check.
	.globl	ba_store_reload
	.type	ba_store_reload, @function
ba_store_reload:                ;! long(size_t)
	subq	$64, %rsp
	movabsq	$0x5a5a5a5a5a5a5a5a, %r10
	movq	%r10, 8(%rsp) #! stack buffer (s, %rsp, %rsp + 32)
	leaq	8(%rsp), %rbx
	movq	%rbx, 40(%rsp)
	movq	40(%rsp), %rax
	movl	(%rax), %eax #! stack buffer (s)
	addq	$64, %rsp
	ret
	.size	ba_store_reload, .-ba_store_reload

# The buffer address stored through a REAL HEAP POINTER (the C helper hands us
# a writable heap object) and reloaded: the address survives the round trip as
# an ordinary integer and the by-address check passes.
	.globl	ba_heap_roundtrip
	.type	ba_heap_roundtrip, @function
ba_heap_roundtrip:              ;! long(ptr, size_t)
	subq	$64, %rsp
	movabsq	$0x7788990011223344, %r10
	movq	%r10, 8(%rsp) #! stack buffer (h, %rsp, %rsp + 32)
	leaq	8(%rsp), %rbx
	movq	%rdi, %rcx
	addq	$16, %rcx
	movq	%rbx, (%rcx)
	movq	(%rcx), %rax
	movl	(%rax), %eax #! stack buffer (h)
	addq	$64, %rsp
	ret
	.size	ba_heap_roundtrip, .-ba_heap_roundtrip

# A cmov selects between the buffer address and a heap address; the annotated
# access runs the runtime check on whichever value was selected (the buffer
# path passes; the heap value is only selected when the caller asks for it and
# fails the check loudly).
	.globl	ba_cmov
	.type	ba_cmov, @function
ba_cmov:                        ;! long(ptr, size_t)
	subq	$64, %rsp
	movabsq	$0x6a6a6a6a6a6a6a6a, %r10
	movq	%r10, 8(%rsp) #! stack buffer (c, %rsp, %rsp + 32)
	leaq	8(%rsp), %rbx
	testq	%rsi, %rsi
	jz	.keep
	movq	%rdi, %rbx
.keep:
	movl	(%rbx), %eax #! stack buffer (c)
	addq	$64, %rsp
	ret
	.size	ba_cmov, .-ba_cmov

# A walking pointer (the rsaz MUL shape): the long form on the static seed
# store declares the buffer; the address starts at the buffer's base and is
# bumped 8 bytes per store; every store through the walking pointer gets its
# own runtime check. Every byte read back was written first.
	.globl	ba_walk
	.type	ba_walk, @function
ba_walk:                        ;! long(size_t)
	subq	$96, %rsp
	movq	$0, 8(%rsp) #! stack buffer (w, %rsp, %rsp + 80)
	leaq	8(%rsp), %rax
	movq	$1, (%rax) #! stack buffer (w)
	movq	$2, 8(%rax) #! stack buffer (w)
	leaq	8(%rax), %rax
	movq	$3, (%rax) #! stack buffer (w)
	movq	$4, 8(%rax) #! stack buffer (w)
	movq	8(%rsp), %rcx #! stack buffer (w)
	movq	16(%rsp), %rdx #! stack buffer (w)
	movq	24(%rsp), %r8 #! stack buffer (w)
	movq	%rcx, %rax
	shlq	$16, %rdx
	orq	%rdx, %rax
	shlq	$24, %r8
	orq	%r8, %rax
	addq	$96, %rsp
	ret
	.size	ba_walk, .-ba_walk

# A masked ({%kN}) by-address move: the FULL width is bounds-checked, then the
# masked move lowers through the scratch register (masked-off lanes keep their
# memory bytes).
	.globl	ba_masked
	.type	ba_masked, @function
ba_masked:                      ;! long(size_t)
	subq	$128, %rsp
	vmovdqu64	.Lbapat(%rip), %zmm0
	vmovdqu64	%zmm0, 0(%rsp) #! stack buffer (m, %rsp, %rsp + 128)
	vmovdqu64	%zmm0, 64(%rsp) #! stack buffer (m)
	movl	$15, %eax
	kmovw	%eax, %k1
	vmovdqu64	.Lbapat2(%rip), %zmm1
	leaq	8(%rsp), %rax
	vmovdqu64	%zmm1, (%rax){%k1} #! stack buffer (m)
	movq	8(%rsp), %r11 #! stack buffer (m)
	movq	64(%rsp), %rbx #! stack buffer (m)
	movq	%r11, %rax
	subq	%rbx, %rax
	addq	$128, %rsp
	ret
	.size	ba_masked, .-ba_masked
# A masked ({%k1}) by-address move whose base is an ORDINARY web (an ALU op
# between the lea and the access knocks the web out of the carrier path): the
# full width is bounds-checked, then the masked move lowers through the
# frame pass's scratch register (masked-off lanes keep their memory bytes).
	.globl	ba_masked_walk
	.type	ba_masked_walk, @function
ba_masked_walk:                 ;! long(size_t)
	subq	$128, %rsp
	vmovdqu64	.Lbapat(%rip), %zmm0
	vmovdqu64	%zmm0, 0(%rsp) #! stack buffer (n, %rsp, %rsp + 128)
	vmovdqu64	%zmm0, 64(%rsp) #! stack buffer (n)
	movl	$15, %eax
	kmovw	%eax, %k1
	vmovdqu64	.Lbapat2(%rip), %zmm1
	leaq	8(%rsp), %rax
	addq	$0, %rax
	vmovdqu64	%zmm1, (%rax){%k1} #! stack buffer (n)
	movq	8(%rsp), %rax #! stack buffer (n)
	addq	$128, %rsp
	ret
	.size	ba_masked_walk, .-ba_masked_walk
	.section	.rodata
	.align	64
.Lbapat:
	.quad	17,17,17,17,17,17,17,17
.Lbapat2:
	.quad	34,34,34,34,34,34,34,34
	.text
	.section	.note.GNU-stack,"",@progbits
