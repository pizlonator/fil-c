	.include "sysreg-test-utils.inc"

	.text

	.arch armv8-a+lor

	rw_sys_reg sys_reg=lorc_el1
	rw_sys_reg sys_reg=lorea_el1
	rw_sys_reg sys_reg=lorn_el1
	rw_sys_reg sys_reg=lorsa_el1
	rw_sys_reg sys_reg=icc_ctlr_el3
	rw_sys_reg sys_reg=icc_sre_el1
	rw_sys_reg sys_reg=icc_sre_el2
	rw_sys_reg sys_reg=icc_sre_el3

	rw_sys_reg sys_reg=ich_vtr_el2 w=0
