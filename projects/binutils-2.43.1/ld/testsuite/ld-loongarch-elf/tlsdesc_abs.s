	.section ".tdata", "awT", @progbits
	.global var
var:	.dword 1
	.text
	# No matter which register the user uses, the abs macro expansion uses $a0
	la.tls.desc	$a0,var
	la.tls.desc	$t0,var
