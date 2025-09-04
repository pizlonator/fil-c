	.include "sysreg-test-utils.inc"

	.text

	rw_sys_reg sys_reg=pmovsclr_el0
	rw_sys_reg sys_reg=pmovsset_el0

	rw_sys_reg sys_reg=id_aa64afr0_el1 w=0
	rw_sys_reg sys_reg=id_aa64afr1_el1 w=0
	rw_sys_reg sys_reg=id_aa64dfr0_el1 w=0
	rw_sys_reg sys_reg=id_aa64dfr1_el1 w=0
	rw_sys_reg sys_reg=id_aa64dfr2_el1 w=0
	rw_sys_reg sys_reg=id_dfr0_el1 w=0
	rw_sys_reg sys_reg=id_pfr0_el1 w=0
	rw_sys_reg sys_reg=id_pfr1_el1 w=0
	rw_sys_reg sys_reg=id_afr0_el1 w=0
	rw_sys_reg sys_reg=id_mmfr0_el1 w=0
	rw_sys_reg sys_reg=id_mmfr1_el1 w=0
	rw_sys_reg sys_reg=id_mmfr2_el1 w=0
	rw_sys_reg sys_reg=id_mmfr3_el1 w=0
	rw_sys_reg sys_reg=id_mmfr4_el1 w=0
	rw_sys_reg sys_reg=id_isar0_el1 w=0
	rw_sys_reg sys_reg=id_isar1_el1 w=0
	rw_sys_reg sys_reg=id_isar2_el1 w=0
	rw_sys_reg sys_reg=id_isar3_el1 w=0
	rw_sys_reg sys_reg=id_isar4_el1 w=0
	rw_sys_reg sys_reg=id_isar5_el1 w=0

	rw_sys_reg sys_reg=id_aa64isar2_el1 xreg=x0 w=0
	rw_sys_reg sys_reg=id_aa64isar2_el1 xreg=x30 w=0
	rw_sys_reg sys_reg=id_aa64isar3_el1 xreg=x0 w=0
	rw_sys_reg sys_reg=id_aa64isar3_el1 xreg=x30 w=0

	rw_sys_reg sys_reg=s3_0_c12_c15_0 w=0
	rw_sys_reg sys_reg=s3_0_c4_c11_0
	rw_sys_reg sys_reg=s2_1_c0_c3_0
