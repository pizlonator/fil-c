# Testcase for a variety of change of flow instructions
# Because some of these are indirect branches, SCFI will bail out
# with an error.  This test merely checks that the ginsn creation
# process can handle these insns gracefully.
	.text
	.align  2
	.global foo
	.type   foo, %function
foo:
	bl      dump_bt
	b       .L3
	br      x17
.L3:
	blr     x3
	ret
	.size   foo, .-foo
