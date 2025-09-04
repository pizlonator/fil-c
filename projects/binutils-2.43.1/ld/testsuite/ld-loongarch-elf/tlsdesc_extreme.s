	.section ".tdata", "awT", @progbits
	.global var
var:	.dword 1
	.text
	# No matter which two registers are passed in, $a0 and $ra are always used
	la.tls.desc	$a0,$ra,var
	la.tls.desc	$t0,$t1,var
