// Test DT_RELR with references in discarded sections.

.text
.p2align 3
.global _start
_start:
	nop

sym_local:
	nop

.global sym_hidden
.hidden sym_hidden
sym_hidden:
	nop

.global sym_global
sym_global:
	nop

.global sym_global_abs
.set sym_global_abs, 42

.global sym_weak_undef
.weak sym_weak_undef

.section .discard.got_local,"ax"
	adrp	x0, :got:sym_local
	ldr	x0, [x0, :got_lo12:sym_local]

.section .discard.got_global,"ax"
	adrp	x0, :got:sym_global
	ldr	x0, [x0, :got_lo12:sym_global]

.section .discard.local,"a"
.p2align 1
discard_local:
.xword sym_local

.section .discard.hidden,"a"
.p2align 1
discard_hidden:
.xword sym_hidden

.section .discard.global,"a"
.p2align 1
discard_global:
.xword sym_global

.section .discard.global_abs,"a"
.p2align 1
discard_global_abs:
.xword sym_global_abs

.section .discard.weak_undef,"a"
.p2align 1
discard_weak_undef:
.xword sym_weak_undef

.section .discard._DYNAMIC,"a"
.p2align 1
discard_DYNAMIC:
.xword _DYNAMIC
