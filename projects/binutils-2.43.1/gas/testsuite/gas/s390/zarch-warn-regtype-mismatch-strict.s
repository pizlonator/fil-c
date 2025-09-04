.text
foo:
	la	%f1,16(%f2,%f3)		# la	%r1,16(%r2,%r3)
	ld	%r1,16(%f2,%f3)		# ld	%f1,16(%r2,%r3)
	vl	%r1,16(%f2,%f3),0	# vl	%v1,16(%r2,%r3),0
	vgef	%r1,16(%r1,%f3),0	# vgef	%v1,16(%v2,%r3),0
	sar	%r1,%a2			# sar	%a1,%r2
