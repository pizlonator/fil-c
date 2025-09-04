.text

	ldiapp x0, x1, [x3]
	ldiapp w0, w1, [x3]
	ldiapp x0, x1, [x3], #16
	ldiapp w0, w1, [x3], #8
	ldiapp x0, x1, [x30], #16
	ldiapp x29, x30, [x0], #16
	ldiapp xzr, x1, [x3], #16
	ldiapp wzr, w1, [x3], #8
	ldiapp x0, x1, [sp], #16
	ldiapp w0, w1, [sp], #8
	// Note: the following examples have a register overlap between source and
	// destination registers, but the doc mentions that, in the case where no
	// offset is specified, writeback is disabled, and so the writeback overlap
	// for load is fine.
	ldiapp x0, x1, [x0]
	ldiapp x0, x1, [x1]
	ldiapp w0, w1, [x0]
	ldiapp w0, w1, [x1]

	stilp x0, x1, [x3]
	stilp w0, w1, [x3]
	stilp x0, x1, [x3, #-16]!
	stilp w0, w1, [x3, #-8]!
	// Note: the following examples have a register overlap between source and
	// destination registers, but the doc mentions that, in the case where no
	// offset is specified, writeback is disabled, and so the writeback overlap
	// for store is fine.
	stilp x0, x1, [x1]
	stilp x0, x1, [x0]
	stilp w0, w1, [x1]
	stilp w0, w1, [x0]

	ldapr w0, [x1]
	ldapr w0, [x1, #0]
	ldapr x0, [x1]
	ldapr x0, [x1, #0]
	ldapr w1, [x2], #4
	ldapr x1, [x2], #8
	ldapr x30, [x0], #8
	ldapr x0, [x30], #8
	ldapr wzr, [x1], #4
	ldapr xzr, [x1], #8
	ldapr w0, [sp], #4
	ldapr x0, [sp], #8

	stlr w0, [x1]
	stlr w0, [x1, #0]
	stlr x0, [x1]
	stlr x0, [x1, #0]
	stlr w1, [x2, #-4]!
	stlr x1, [x2, #-8]!
	stlr x30, [x0, #-8]!
	stlr x0, [x30, #-8]!
	stlr wzr, [x1, #-4]!
	stlr xzr, [x1, #-8]!
	stlr w0, [sp, #-4]!
	stlr x0, [sp, #-8]!
