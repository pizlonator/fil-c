# Testcase with a branch instruction with symbol and an addend
# Such insns make CFG creation difficult and hence make the
# function ineligible for SCFI.
	.type   foo, %function
foo:
	b       symbol+1
	ret
	.size   foo, .-foo
