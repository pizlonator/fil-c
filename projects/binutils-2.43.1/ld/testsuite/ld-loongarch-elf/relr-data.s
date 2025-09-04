# Test symbol references in .data when used with DT_RELR.
# Relocations for unaligned sections are currently not packed.

.text
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

.section .data.unaligned_local
unaligned_local:
.quad sym_local

.section .data.unaligned_hidden
unaligned_hidden:
.quad sym_hidden

.section .data.unaligned_global
unaligned_global:
.quad sym_global

.section .data.unaligned_DYNAMIC
unaligned_DYNAMIC:
.quad _DYNAMIC

.section .data.aligned_local
.p2align 1
aligned_local:
.quad sym_local

.section .data.aligned_hidden
.p2align 1
aligned_hidden:
.quad sym_hidden

.section .data.aligned_global
.p2align 1
aligned_global:
.quad sym_global

.section .data.aligned_global_abs
.p2align 1
aligned_global_abs:
.quad sym_global_abs

.section .data.aligned_weak_undef
.p2align 1
aligned_weak_undef:
.quad sym_weak_undef

.section .data.aligned_DYNAMIC
.p2align 1
aligned_DYNAMIC:
.quad _DYNAMIC
