# UNSOUND: a yolo stack-argument read through an ADJUSTED entry-rsp alias. The
# `leaq 0(%rsp), %r9` parks the entry rsp in %r9 (a genuine yolo alias), but
# the `addq $8, %r9` that follows reads the save and redefines the register —
# sarcasm cannot prove the read of the parked save safe (the rewrite drops the
# save, so the register's stack value does not exist in the output), and the
# adjusted register would no longer name the entry rsp anyway: the `movq 32(%r9)`
# load claims to read the incoming 7th (stack-passed) argument through the
# alias, but hardware would address entry_rsp + 40 — caller frame memory the
# signature gives no argument for. analyzeFrame rejects the shape fail-closed
# at the read of the save register:
#   sarcasm: ...: reading %r9 while it holds the prologue `leaq 0(%rsp), %r9`
#   save cannot be proven safe (the save is dropped by the rewrite — the
#   register's stack value does not exist in the output; recover %rsp from it
#   first or redefine it): addq $8, %r9
# (Before the nil-depth guard fix for issue #366 the fpYolo scan's sibling
# checks already rejected this shape the same way; the rejection must hold
# with and without the fix.)
	.text
	.globl	adjusted
	.type	adjusted, @function
adjusted:                       #! long(ptr,ptr,ptr,ptr,long,ptr,long)
	subq	$32, %rsp
	leaq	0(%rsp), %r9
	addq	$8, %r9
	movq	32(%r9), %rsi
	addq	$32, %rsp
	rep ret
	.size	adjusted, .-adjusted
	.section	.note.GNU-stack,"",@progbits
