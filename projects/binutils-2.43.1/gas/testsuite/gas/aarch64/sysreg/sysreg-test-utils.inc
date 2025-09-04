/** Test util to perform a READ and/or WRITE from/to a system register
 *
 * \param sysreg a system register.
 * \param xreg   any general purpose register Xn (default=x0).
 * \param r      boolean enabling or disabling the read test (default=1).
 * \param w      boolean enabling or disabling the write test (default=1).
 */
.macro rw_sys_reg sys_reg xreg=x0 r=1 w=1
.ifc \w, 1
msr \sys_reg, \xreg
.endif
.ifc \r, 1
mrs \xreg, \sys_reg
.endif
.endm

/** 128-bits version of rw_sys_reg
 *
 * \param sysreg a system register.
 * \param xreg1  first general-purpose destination register Xn (default=x0).
 * \param xreg2  second general-purpose destination register Xn (default=x1).
 * \param r      boolean enabling or disabling the read test (default=1).
 * \param w      boolean enabling or disabling the write test (default=1).
 */
.macro rw_sys_reg_128 sys_reg xreg1=x0 xreg2=x1 r=1 w=1
.ifc \w, 1
msrr \sys_reg, \xreg1, \xreg2
.endif
.ifc \r, 1
mrrs \xreg1, \xreg2, \sys_reg
.endif
.endm
