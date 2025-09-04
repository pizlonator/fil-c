	.file	"subalign.c"
	.text
	.globl		a_one
	.section	.mysection1,"aw"
	.align 8
a_one:
	.zero		1

	.globl		b_one
	.section	.mysection2,"aw"
	.align 		8
b_one:
	.zero	1

	.globl		c_one
	.section	.mysection3,"aw"
	.align 		8
c_one:
	.zero	1

