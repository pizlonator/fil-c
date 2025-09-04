	.data
	.section	.tdata,"awT",@progbits
	.fill 0x1000,1,0
var:
	.word 1
	.text
	.global	fn1
	.type	gn1,@function
fn1:
	# expect IE to relax LE.
	la.tls.ie $a0,var
