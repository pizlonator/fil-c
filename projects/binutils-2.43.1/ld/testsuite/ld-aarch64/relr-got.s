// Test GOT relocations with DT_RELR.

.text
.global _start
_start:
	adrp	x0, :got:sym_local
	ldr	x0, [x0, :got_lo12:sym_local]

	adrp	x0, :got:sym_hidden
	ldr	x0, [x0, :got_lo12:sym_hidden]

	adrp	x0, :got:sym_global
	ldr	x0, [x0, :got_lo12:sym_global]

	adrp	x0, :got:sym_global
	ldr	x0, [x0, :got_lo12:sym_global_abs]

	adrp	x0, :got:sym_weak_undef
	ldr	x0, [x0, :got_lo12:sym_weak_undef]

	adrp	x0, :got:_DYNAMIC
	ldr	x0, [x0, :got_lo12:_DYNAMIC]

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
