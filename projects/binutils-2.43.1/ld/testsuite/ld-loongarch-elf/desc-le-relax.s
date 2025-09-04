	.global var
	.section .tdata,"awT",@progbits
var1:
	.word 1
var2:
	.word 1
	.text
	.global	fn1
	.type	fn1,@function
fn1:

	# DESC will relax to LE.
	la.tls.desc $a0,var1
	la.tls.desc $a0,var2
