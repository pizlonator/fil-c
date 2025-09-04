.include "sysreg-test-utils.inc"

.text

/* Delegated SError exceptions for EL3. */
rw_sys_reg sys_reg=vdisr_el3 xreg=x0 r=1 w=1
rw_sys_reg sys_reg=vsesr_el3 xreg=x0 r=1 w=1

/* System Performance Monitors Extension version 2. */
rw_sys_reg sys_reg=spmzr_el0 xreg=x0 r=1 w=1

/* Enhanced Software Step Extension. */
rw_sys_reg sys_reg=mdstepop_el1 xreg=x0 r=1 w=1
