	.section	.tbss,"awT",@nobits
	.global sg1
sg1:
	.zero	4
sl1:
	.zero	4

	.text
	.globl	_start
	.type	_start,@function
_start:
.desc1:
	auipc	a0, %tlsdesc_hi(sg1)
	lw	t0, %tlsdesc_load_lo(.desc1)(a0)
	addi	a0, a0, %tlsdesc_add_lo(.desc1)
	jalr	t0, t0, %tlsdesc_call(.desc1)

.desc2:
	auipc	a0, %tlsdesc_hi(sl1)
	lw	t0, %tlsdesc_load_lo(.desc2)(a0)
	addi	a0, a0, %tlsdesc_add_lo(.desc2)
	jalr	t0, t0, %tlsdesc_call(.desc2)

	ret
