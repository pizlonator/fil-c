	.data
	.section	.tdata,"awT",@progbits
var1:
	.word 1
var2:
	.word 2
	.text
	.global	fn1
	.type	gn1,@function
fn1:
	# expect IE to relax LE
	la.tls.ie $a0,var1
	la.tls.ie $a0,var2
