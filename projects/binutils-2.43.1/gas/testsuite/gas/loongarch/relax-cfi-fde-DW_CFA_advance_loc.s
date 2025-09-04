# Emits ADD/SUB relocations for CFA FDE DW_CFA_advance_loc with -mrelax option.
.text
.cfi_startproc

.cfi_def_cfa 22, 0
la.local $t0, a
.cfi_restore 22

.cfi_def_cfa 22, 0
la.got $t0, a
.cfi_restore 22

.cfi_def_cfa 22, 0
la.tls.ld $t0, a
.cfi_restore 22

.cfi_def_cfa 22, 0
la.tls.gd $t0, a
.cfi_restore 22

.cfi_def_cfa 22, 0
la.tls.desc $t0, a
.cfi_restore 22

.cfi_def_cfa 22, 0
pcalau12i $t0, %le_hi20_r(a)
.cfi_restore 22

.cfi_def_cfa 22, 0
add.d $t0, $tp, $t0, %le_add_r(a)
.cfi_restore 22

.cfi_def_cfa 22, 0
la.tls.ie $t0, a
.cfi_restore 22

.cfi_def_cfa 22, 0
la.tls.le $t0, a
.cfi_restore 22

.cfi_def_cfa 22, 0
call36 f
.cfi_restore 22

.cfi_endproc
