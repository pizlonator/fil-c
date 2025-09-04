# Testcase with an SVE instruction with Z register usage
# of a callee-saved register.  Currently these are not handled
# and the SCFI machinery must error out.
	.arch   armv8-a+sve
	.type   foo, %function
foo:
	st1d    z8.d, p2, [sp, #1, mul vl]
	.size   foo, .-foo
