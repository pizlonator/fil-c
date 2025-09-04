	.data
	.section	.tdata,"awT",@progbits
	.global gl1, gl2, gl3, gl4
	.global hd1, hd2
	.hidden hd1, hd2
gl1:	.dword 1
gl2:	.dword 2
gl3:	.dword 3
gl4:	.dword 4
lo1:	.dword 10
lo2:	.dword 20
lo3:	.dword 30
lo4:	.dword 40
hd1:	.dword 100
hd2:	.dword 200
	.text
# Access global symbol
fun_gl1:
	# GD + IE
	# GD: pcaddi + addi.d => pcaddi
	la.tls.gd	$a0, gl1
	la.tls.ie	$a0, gl1

	# GD + DESC
	# GD: pcaddi + addi.d => pcaddi
	la.tls.gd	$a0, gl2
	# DESC: pcaddi + addi.d => pcaddi
	la.tls.desc	$a0, gl2

	# DESC + IE
	# DESC -> IE
	la.tls.desc	$a0, gl3
	la.tls.ie	$a0, gl3

	# GD + DESC + IE
	# GD: pcaddi + addi.d => pcaddi
	la.tls.gd	$a0, gl4
	# DESC: pcaddi + addi.d => pcaddi
	la.tls.desc	$a0, gl4
	la.tls.ie	$a0, gl4

# Access local symbol
fun_lo:
	# IE + GD
	la.tls.ie	$a0, lo1
	# GD: pcaddi + addi.d => pcaddi
	la.tls.gd	$a0, lo1

	# DESC + GD
	# DESC: pcaddi + addi.d => pcaddi
	la.tls.desc	$a0, lo2
	# GD: pcaddi + addi.d => pcaddi
	la.tls.gd	$a0, lo2

	# DESC + IE
	# DESC: DESC -> IE
	la.tls.desc	$a0, lo3
	la.tls.ie	$a0, lo3

	# DESC + GD + IE
	# DESC: pcaddi + addi.d => pcaddi
	la.tls.desc	$a0, lo4
	# GD: pcaddi + addi.d => pcaddi
	la.tls.gd	$a0, lo4
	la.tls.ie	$a0, lo4

# Access external undef symbol
fun_external:
	la.tls.desc	$a0, sH1

# Access hidden symbol
fun_hidden:
	la.tls.desc	$a0, hd1
	la.tls.desc	$a0, hd2
