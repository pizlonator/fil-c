.text
.global _start
_start:
	la.got	$a0, sym_local
	la.got	$a1, sym_hidden
	la.got	$a2, sym_global
	la.got	$a3, sym_global_abs
	la.got	$a4, sym_weak_undef
	la.got	$a5, _DYNAMIC

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
