# D9 fixed-frame escape promotion COEXISTING with the `.alloca` directive in
# ONE function. The escape cluster (`leaq 16(%rsp), %rax` + copies/offset leas
# + `call alhelper ;! long(ptr,ptr)`) promotes the whole fixed frame to ONE GC
# allocation (a filc_allocate in the sarcasm output — expected here); the
# `.alloca $32, $16, %r10` side buffer is an INDEPENDENT GC allocation (a
# SECOND filc_allocate — a stale DESIGN.md claim said the combination was
# rejected; it is not). The asm:
#   * writes distinct patterns into the promoted frame via direct slots AND
#     derived pointers (aliased overwrites that must land in one home);
#   * writes a different pattern into the alloca buffer via its result
#     register;
#   * hands BOTH pointers to the C helper, which reads the patterns back,
#     writes through both pointers, and returns a sum;
#   * reads everything back through direct slots, fresh derived pointers, the
#     alloca result register, AND a register copy of it made before the call
#     (the alloca capability must survive the call through a spilled copy).
# The checksum is exact (51423) and mirrored in the C driver.
	.text
	.globl	alesc
	.type	alesc, @function
alesc:                          #! long(long)
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	subq	$80, %rsp           # fixed frame [0,80): D0 = 104, region [0,80)

	# --- the escape cluster: the lea escapes to alhelper -> promotion ---
	leaq	16(%rsp), %rax      # region+16 — the ESCAPING lea
	movq	%rax, %rbx          # park the region+16 pointer (call-safe)

	# --- the `.alloca` side buffer: an INDEPENDENT GC allocation ---
	.alloca $32, $16, %r10      # r10 = the side buffer (32 bytes, 16-aligned)

	# --- patterns into the promoted frame (direct slots AND derived
	# pointers over the same homes) ---
	movq	%rdi, %r12          # park the argument (rdi is a call-clobbered arg reg)
	movq	%rdi, %r13          # park it twice: the checksum needs it at the end
	leaq	1(%rdi), %rcx
	movq	%rcx, 16(%rsp)      # slot16 = arg+1 (direct)
	movq	%rcx, (%rax)        # region+16 = arg+1 (derived: same home)
	leaq	2(%rdi), %rcx
	movq	%rcx, 24(%rsp)      # slot24 = arg+2 (direct)
	movq	%rcx, 8(%rax)       # region+24 = arg+2 (derived: same home)
	leaq	3(%rdi), %rcx
	movq	%rcx, 8(%rax)       # region+24 = arg+3 (derived overwrite)
	movq	$4, 32(%rsp)        # slot32 = 4
	movq	$5, 40(%rsp)        # slot40 = 5

	# --- a different pattern into the alloca buffer (via its result
	# register) ---
	movq	$100, (%r10)
	movq	$200, 8(%r10)
	movq	$300, 16(%r10)
	movq	$400, 24(%r10)
	movq	%r10, %r11          # a register COPY of the alloca pointer, live
	                            # across the call
	xorl	%ebx, %ebx          # the checksum accumulator (rbx freed: re-derived
	                            # below — the helper's arg2 rides %rsi instead)

	# --- hand BOTH pointers to the C helper ---
	leaq	16(%rsp), %rsi      # arg2 = region+16 (fresh derived pointer)
	movq	%r10, %rdi          # arg1 = the alloca buffer
	call	alhelper ;! long(ptr,ptr)

	# --- read everything back ---
	movq	%rax, %r12          # the helper's return
	leaq	16(%rsp), %rcx      # a fresh derived region+16 pointer
	movq	(%rcx), %rbx        # 8000 via the derived pointer (the helper's write)
	addq	16(%rsp), %rbx      # + 8000 via the direct slot (one home)
	addq	8(%rcx), %rbx       # + 903 via the derived pointer (region+24)
	addq	24(%rsp), %rbx      # + 903 via the direct slot (one home)
	addq	32(%rsp), %rbx      # + 4
	addq	40(%rsp), %rbx      # + 5
	# the alloca buffer, via the result register and the register copy:
	movq	(%r10), %rcx        # 7000 (the helper's write through the alloca pointer)
	addq	%rcx, %rbx
	movq	8(%r10), %rcx       # 200
	addq	%rcx, %rbx
	movq	16(%r10), %rcx      # 300
	addq	%rcx, %rbx
	movq	24(%r10), %rcx      # 400
	addq	%rcx, %rbx
	movq	(%r11), %rcx        # 7000 via the register COPY (the alloca capability
	                            # survived the call through a spilled copy)
	addq	%rcx, %rbx
	addq	%r12, %rbx          # + the helper's return
	addq	%r13, %rbx          # + the argument, once
	movq	%rbx, %rax
	addq	$80, %rsp
	popq	%r13
	popq	%r12
	popq	%rbx
	ret
	.size	alesc, .-alesc
	.section	.note.GNU-stack,"",@progbits
