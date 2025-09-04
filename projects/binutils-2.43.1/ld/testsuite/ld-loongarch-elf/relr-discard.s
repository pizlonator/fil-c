# Test DT_RELR with references in discarded sections.

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
	la.got $a0, sym_local

.section .discard.got_global,"ax"
	la.got $a0, sym_global

.section .discard.local,"a"
.p2align 1
discard_local:
.quad sym_local

.section .discard.hidden,"a"
.p2align 1
discard_hidden:
.quad sym_hidden

.section .discard.global,"a"
.p2align 1
discard_global:
.quad sym_global

.section .discard.global_abs,"a"
.p2align 1
discard_global_abs:
.quad sym_global_abs

.section .discard.weak_undef,"a"
.p2align 1
discard_weak_undef:
.quad sym_weak_undef

.section .discard._DYNAMIC,"a"
.p2align 1
discard_DYNAMIC:
.quad _DYNAMIC
