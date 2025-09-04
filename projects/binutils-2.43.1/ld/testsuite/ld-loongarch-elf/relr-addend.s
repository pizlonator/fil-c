.data
.align	8
x:
	.quad	0x114514
y:
	.quad	0x1919810
px:
	.quad	x
py:
	.quad	y

.text
.align	2
_start:
	la.got	$a0, x
	la.got	$a1, y
	ret
