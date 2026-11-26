# Regression test for the sarcasm crash in the yolo stack-argument alias scan
# (GitHub issue #366): the `addq $8, %r9` VALUE use of the first
# `leaq 8(%rsp), %r9` alias defeats the frame-escape detector's
# memory-only-use check, so the fixed frame is promoted to a GC frame region;
# in the rewrite's analyzeFrame run BOTH `leaq 8(%rsp)` aliases then classify
# as REGION POINTERS, and their smap entries carry NO depth (a region pointer
# is <region base + K> — a real pointer, by definition not a saved entry rsp;
# conflict entries are the other depth-less shape). The fpYolo scan used to
# call yoloArgSlotOf with that nil depth and crashed with `attempt to perform
# arithmetic (sub) on number and nil` at x86_64_frame.luau:623. A depth-less
# entry can never be the base of a genuine yolo stack-argument read (a genuine
# alias always parks a numeric depth), so the scan now skips slot/roff/conflict
# entries and yoloArgSlotOf itself rejects a nil saveDepth.
# Runtime shape: %rbx is parked at frame+8 (leaq 8(%rsp) after the 32-byte
# subq), so `8(%rbx)` reads the same frame word as `16(%rsp)` — the slot the
# first (integer) argument was stored into. The function returns that value,
# so the driver can pin the load with a known constant.
	.text
	.globl	regionload
	.type	regionload, @function
regionload:                     #! long(long,ptr,ptr,ptr,long,ptr,long)
	subq	$32, %rsp
	leaq	8(%rsp), %r9
	addq	$8, %r9
	leaq	8(%rsp), %rbx
	movq	%rdi, 16(%rsp)      # frame offset 16 (from rsp): the known value
	movq	8(%rbx), %rsi       # rbx = frame+8; reads the same frame word
	movq	%rsi, %rax
	addq	$32, %rsp
	rep ret
	.size	regionload, .-regionload
	.section	.note.GNU-stack,"",@progbits
