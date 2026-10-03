# UNSOUND: a mem-dest `xaddq` — a claimed-GPR instruction — aliasing an

# outstanding non-spill (dropped) push's save slot inside the transient prologue

# pad, with a mid-window read of the pushed register. `pushq %r12` parks the

# seed 900 and the call's outgoing word 0 shares that save slot; the aliased

# access is NOT one of the mem-dest ALU RMW openers the save-slot alias model

# rewrites onto the pushed register (addq/notq — see

# sarcasm-frame-escape-push-alias-rmw-reject-att), because the FP knowledge

# table CLAIMS xadd for its exact GPR semantics (the old-value writeback: xadd

# is a read-modify-write on BOTH of its operands), so isFpInsn is true for it

# PURELY by the claim. That one gate excluded xadd from BOTH the alias model's

# candidate scan AND analyzeFrame's poisoned-window validation pass, so this

# shape used to be ACCEPTED with no diagnostic and silently miscompiled: the

# access virtualized the pad slot like an ordinary slot (the emitted code was

# `xaddq %rbp,48(%rsp)` against output-frame memory never initialized with the

# pushed value), while the pad's content lives in the pushed register's web,

# not in memory. Native: outgoing word 0 (arg7) = 900 + the region pointer (the

# RMW'd slot content) and arg3 = 900 (the old-value writeback into rbx); the

# miscompiled build served arg7 = 900 (the pushed web) and arg3 = the region

# pointer (the writeback lost). Fixed by failing closed: the rewrite rejects

# EVERY claimed-GPR access (an fp EXACT entry with `gpr` and a claim whose

# statement names no FP/vector register — xaddb/w/l/q, crc32, adcx/adox,

# bsf/bsr, the BMI2 register forms, lzcnt/tzcnt/popcnt, movnti, ...) that

# overlaps an outstanding non-spill save slot, with the alias model's standard

# rejection style:

#   sarcasm: stack access overlapping a pushed register's save slot inside the

#   transient prologue pad by an instruction the save-slot model cannot express

#   (`xaddq` is a pure-GPR instruction the FP knowledge table claims for its

#   exact register semantics, so the alias model never rewrites it onto the

#   pushed register: the access would ride an ordinary slot web while the pad's

#   content lives in the pushed register's web, not in memory — and xadd's

#   old-value writeback claims BOTH operands): pop the pad first or place the

#   access below the push: xaddq %rbx, (%rsp)

# (The UNPROMOTED variant — the same body without the escaping lea, rbx loaded

# from an ordinary heap pointer instead — rejects with the identical message:

# the claimed-GPR rejection is the alias model's own overlap scan and not a

# region-promotion artifact.)

	.text

	.globl	xaddread

	.type	xaddread, @function

xaddread:                         #! long(ptr,ptr,long)

	pushq	%rbx

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

	subq	$104, %rsp          # D0; region [16,104)

	movq	%rdx, %r12          # park the seed 900

	movq	%rdi, %r13          # bufA

	movq	%rsi, %r14          # bufB

	leaq	16(%rsp), %rax      # the ESCAPING lea (region+0)

	movq	%rax, %rbx          # rbx = region pointer V

	movq	$940, 48(%rsp)

	movq	$950, 56(%rsp)

	pushq	%r12                # save slot IS outgoing word 0; hw r12 = 900

	xaddq %rbx, (%rsp)       # ALIASED claimed-GPR RMW: slot = 900+V, old value -> rbx

	movq	%r12, %r15          # MID-WINDOW READ: native r15 = 900

	movq	%r14, %rdi          # arg1 = bufB

	movq	%r13, %rsi          # arg2 = bufA

	movq	%rbx, %rdx          # arg3 = region+0

	leaq	24(%rsp), %rcx      # arg4

	leaq	32(%rsp), %r8       # arg5

	leaq	40(%rsp), %r9       # arg6

	movq	%r14, 8(%rsp)       # outgoing word 1 = bufB

	                   	    # outgoing word 0 (arg7) = the slot content itself (no store)

	call	sink8 ;! long(ptr,ptr,ptr,ptr,ptr,ptr,ptr,ptr)

	popq	%r12                # dropped pop

	movq	%r15, %rax          # return r15

	addq	$104, %rsp

	popq	%r15

	popq	%r14

	popq	%r13

	popq	%r12

	popq	%rbx

	ret

	.size	xaddread, .-xaddread

	.globl	sink8

	.type	sink8, @function

sink8:                          #! long(ptr,ptr,ptr,ptr,ptr,ptr,ptr,ptr)

	movq	$604, %rax

	ret

	.size	sink8, .-sink8

	.section	.note.GNU-stack,"",@progbits
