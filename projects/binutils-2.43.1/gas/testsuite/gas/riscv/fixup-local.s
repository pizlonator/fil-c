	.global foo
	.global bar
foo:
	la	a0, .LL0
	lh	a0, .LL0
	sh	a0, .LL0, a0
.LL0:
	la	a0, bar
	lh	a0, bar
	sh	a0, bar, a0
	la	a0, foo
	lh	a0, foo
	sh	a0, foo, a0
.LL1:	auipc	a0, %pcrel_hi(.LL3)
	lw	a0, %pcrel_lo(.LL1)(a0)
.LL2:	auipc	a0, %pcrel_hi(.LL3)
	sw	a0, %pcrel_lo(.LL1)(a0)
.LL3:
	ret
