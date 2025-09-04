/* sysreg-3.s Test file for ARMv8.3 system registers.  */

	.include "sysreg-test-utils.inc"

	.text

	rw_sys_reg sys_reg=apiakeylo_el1
	rw_sys_reg sys_reg=apiakeyhi_el1
	rw_sys_reg sys_reg=apibkeylo_el1
	rw_sys_reg sys_reg=apibkeyhi_el1

	rw_sys_reg sys_reg=apdakeylo_el1
	rw_sys_reg sys_reg=apdakeyhi_el1
	rw_sys_reg sys_reg=apdbkeylo_el1
	rw_sys_reg sys_reg=apdbkeyhi_el1

	rw_sys_reg sys_reg=apgakeylo_el1
	rw_sys_reg sys_reg=apgakeyhi_el1
