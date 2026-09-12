# Coexistence proof for inArgWord + spillShift (x86_64_render): nine longs
# arrive SysV-style — the first six in registers, the last three above the
# caller's frame (8/16/24(%rsp) at entry) — while the fast-CC dense view
# packs nine GPR words (the first four in rdx/rcx/r8/r9, the last five in
# the caller's outgoing area). The `and $-64, %rsp` note plus the vmovdqa64
# cluster force layout.alignA = 64, so the incoming-word loads (five in the
# fast-CC view: a4..a8 arrive on the stack even though the .s names only
# a6..a8 at 304/312/320 — a4/a5's %r8/%r9 homes are fed from words 4/5 by
# the entry unpack) must read through the prologue-saved pre-and rsp
# (saveOff(%rsp)) via the dynamic-align two-move sequence:
#   mov saveOff(%rsp), dst ; mov postPushDelta(dst), dst
# (a static rsp-relative displacement reads true-slack garbage on every
# entry residue except one).
#
# Frame-setup-FIRST shape (the compiler style) is load-bearing here, not
# incidental: the frame geometry (D0) is derived from the prologue prefix,
# and a stack-touching instruction ends the prefix — entry reads placed
# before the pushes collapse D0 to 0, which mis-keys every later ordinary
# frame slot (an outgoing `movq %rax, 0(%rsp)` then keys SLOT_BASE-264
# instead of SLOT_BASE+0 and the call reads garbage). So the pushes, sub,
# and and come first and the incoming words are read compiler-style at
# d+8+8*i(%rsp) (304/312/320 below at depth 296), which the yolo-argument
# redirect maps to the incoming words regardless of the and slack.
#
# Frame layout (sub $256, post-and numbering): the vmovdqa64 cluster lives
# at 128..256 — its FP-materialized range, which keeps 64-alignment
# provable (128 is 64-aligned off a 64-aligned base) while leaving the low
# frame free. The low frame MUST stay clear of it: a user store inside an
# FP-materialized range stays REAL memory instead of virtualizing into a
# slot web, so the call's outgoing-argument uses would read undefined webs.
# Hence the four stash slots at 32/40/48/56 and the three outgoing
# stack-argument slots at 0/8/16 (the compiler-style placement the call's
# 7th+ argument uses resolve to).
#
# The same function then makes an annotated 9-argument call, whose outgoing
# stack-argument window drops rsp over the marshalling: any spill
# reload/store regalloc inserts for marshalling sources inside that window
# carries spillShift and is re-based at render time. ~20 webs are live
# across the call (the entry sum, eight K immediates, four stashed slot
# webs), pressuring the allocator to spill marshalling sources inside the
# window — the reject in x86_64_render for spillShift+inArgWord co-occurring
# on ONE node must NOT fire here (disjoint provenances: entry unpack vs.
# window interior), and a green run proves it has no false positives on
# real spill traffic.
#
# PROLOGUE + SAVEOFF ASSERT (grep recipe — run-tests cannot grep, so the
# C driver pins the same property at runtime instead): assemble with
#   pizfix/bin/sarcasm --x86_64 -S <this file> -o out.yolo.s
# then check (a) the prologue saves the post-push rsp into the frame
# (`movq %r11, <saveOff>(%rsp)`-shaped store before the `sub`), (b) five
# `movq <saveOff>(%rsp), %rX` + `movq <delta>(%rX), %rX` two-move pairs
# with deltas 8*#calleeSaved + 8 + 8*w for w=0..4 (a4..a8 — the entry
# unpack loads all five stack-resident words even though the .s spells
# only three reads), and (c) `and $-64, %rsp` precedes the vmovdqa64
# cluster. The runtime sweep in spill9-main.c (128 recursion
# depths covering every mod-64 entry residue) fails deterministically on
# any static-displacement or wrong-saveOff entry load, so the C check IS
# the in-test saveOff assert. It pins the outgoing side the same way: any
# spillShift mis-rebasing (or a repeat of the two D0/outgoing-shape traps
# documented above — entry reads before setup collapsing D0, user stores
# inside the FP-materialized cluster staying real memory) corrupts the
# helper9 arguments or the checksum on every residue.
	.text
	.globl	sum9_spill
	.type	sum9_spill, @function
sum9_spill:                     ;! long(long,long,long,long,long,long,long,long,long)
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	subq	$256, %rsp
	and	$-64, %rsp
	movq	304(%rsp), %r10
	movq	312(%rsp), %r11
	movq	320(%rsp), %rax
	movq	%rax, %rbx
	vpxorq	%zmm0, %zmm0, %zmm0
	vmovdqa64	%zmm0, 128(%rsp)
	vmovdqa64	128(%rsp), %zmm1
	vmovdqa64	%zmm1, 192(%rsp)
	vmovdqa64	192(%rsp), %zmm2
	vmovq	%xmm2, %rax
	addq	%rdi, %rax
	addq	%rsi, %rax
	addq	%rdx, %rax
	addq	%rcx, %rax
	addq	%r8, %rax
	addq	%r9, %rax
	addq	%r10, %rax
	addq	%r11, %rax
	addq	%rbx, %rax
	movq	%rax, %r12
	movq	$1001, %r13
	movq	$1002, %r14
	movq	$1003, %r15
	movq	$1004, %r10
	movq	$1005, %r11
	movq	$1006, %rbx
	movq	$1007, %r8
	movq	$1008, %r9
	movq	%r13, 32(%rsp)
	movq	%r14, 40(%rsp)
	movq	%r15, 48(%rsp)
	movq	%r10, 56(%rsp)
	movq	$1009, %r13
	movq	$1010, %r14
	movq	$1011, %r15
	movq	$1012, %r10
	movq	%r12, %rdi
	movq	32(%rsp), %rsi
	movq	40(%rsp), %rdx
	movq	%r8, %rcx
	movq	%r9, %r8
	movq	48(%rsp), %r9
	movq	56(%rsp), %rax
	movq	%rax, 0(%rsp)
	movq	%r11, %rax
	movq	%rax, 8(%rsp)
	movq	%rbx, %rax
	movq	%rax, 16(%rsp)
	call	helper9 ;! long(long,long,long,long,long,long,long,long,long)
	addq	%r12, %rax
	addq	32(%rsp), %rax
	addq	40(%rsp), %rax
	addq	48(%rsp), %rax
	addq	56(%rsp), %rax
	addq	%r13, %rax
	addq	%r14, %rax
	addq	%r15, %rax
	addq	%r10, %rax
	addq	$256, %rsp
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	ret
	.size	sum9_spill, .-sum9_spill
	.section	.note.GNU-stack,"",@progbits
