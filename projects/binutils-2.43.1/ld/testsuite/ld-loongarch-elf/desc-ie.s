	.global var
	.section .tdata,"awT",@progbits
var:
	.word 1
	.text
	.global	fn1
	.type	fn1,@function
fn1:

	# Use DESC and IE to access the same symbol,
	# DESC will relax to IE.
	la.tls.ie $a0,var
	la.tls.desc $a0,var
