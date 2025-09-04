	.include "sysreg-test-utils.inc"

	.arch armv9.4-a+d128+the

	rw_sys_reg_128 par_el1 xreg1=x2 xreg2=x3
	rw_sys_reg_128 rcwmask_el1 xreg1=x2 xreg2=x3
	rw_sys_reg_128 rcwsmask_el1 xreg1=x2 xreg2=x3
	rw_sys_reg_128 ttbr0_el1 xreg1=x2 xreg2=x3
	rw_sys_reg_128 ttbr0_el12 xreg1=x2 xreg2=x3
	rw_sys_reg_128 ttbr0_el2 xreg1=x2 xreg2=x3
	rw_sys_reg_128 ttbr1_el1 xreg1=x2 xreg2=x3
	rw_sys_reg_128 ttbr1_el12 xreg1=x2 xreg2=x3
	rw_sys_reg_128 ttbr1_el2 xreg1=x2 xreg2=x3
	rw_sys_reg_128 vttbr_el2 xreg1=x2 xreg2=x3
