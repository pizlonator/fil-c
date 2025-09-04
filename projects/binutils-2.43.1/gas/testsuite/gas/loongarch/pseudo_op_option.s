# Gas enables relax by default.
# Push and pop can be nested, and each pop restores the options before
# the most recent push.
	.text
.L1:
	la.pcrel  $a0,x

	.option push
	.option norelax
	la.pcrel  $a0,x

	.option push
	.option relax
	la.pcrel  $a0,x
	.option pop

	la.pcrel  $a0,x
	.option pop
	la.pcrel  $a0,x
