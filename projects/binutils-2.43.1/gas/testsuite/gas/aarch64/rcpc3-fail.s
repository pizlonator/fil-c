.text
	/* Check invalid operands */
	// wrong addressing mode
	ldiapp w0, w1, [x3, #8]
	ldiapp x0, x1, [x3, #16]
	ldiapp w0, w1, [x3, #-8]!
	ldiapp x0, x1, [x3, #-16]!
	// wrong operand 1
	ldiapp sp, x1, [x3], #16
	ldiapp wsp, w1, [x3], #8
	// wrong operand 2
	ldiapp x0, sp, [x3], #16
	ldiapp w0, wsp, [x3], #8
	// wrong base register
	ldiapp x0, x1, [xzr], #16
	ldiapp x0, x1, [wzr], #16
	ldiapp w0, w1, [w3], #8
	// wrong increment amount
	ldiapp x0, x1, [x3], #8
	ldiapp w0, w1, [x3], #16

	// wrong addressing mode
	stilp w0, w1, [x3, #8]
	stilp x0, x1, [x3, #16]
	stilp w0, w1, [x3], #8
	stilp x0, x1, [x3], #16
	// wrong operand 1
	stilp sp, x1, [x3, #-16]!
	stilp wsp, w1, [x3, #-8]!
	// wrong operand 2
	stilp x0, sp, [x3, #-16]!
	stilp w0, wsp, [x3, #-8]!
	// wrong base register
	stilp x0, x1, [xzr, #-16]!
	stilp x0, x1, [wzr, #-16]!
	stilp w0, w1, [w3, #-8]!
	// wrong increment amount
	stilp w0, w1, [x3, #-16]!
	stilp x0, x1, [x3, #-8]!
	stilp w0, w1, [x3, #16]!
	stilp x0, x1, [x3, #8]!

	// wrong addressing mode
	ldapr w0, [x1, #4]
	ldapr x0, [x1, #8]
	ldapr w0, [x1, #-4]!
	ldapr x0, [x1, #-8]!
	// wrong operand 1
	ldapr wsp, [x0], #4
	ldapr sp, [x0], #8
	// wrong base register
	ldapr x0, [wzr], #8
	ldapr x0, [xzr], #8
	ldapr x0, [w1], #8
	// wrong increment amount
	ldapr w0, [x1], #8
	ldapr x0, [x1], #4

	// wrong addressing mode
	stlr w0, [x1], #4
	stlr x0, [x1], #8
	// wrong operand 1
	stlr wsp, [x0, #-4]!
	stlr sp, [x0, #-8]!
	// wrong base register
	stlr x0, [xzr, #-8]!
	stlr x0, [wzr, #-8]!
	stlr x0, [w1, #-8]!
	// wrong immediate offset
	stlr w0, [x1, #4]
	stlr x0, [x1, #8]
	// wrong increment amount
	stlr w0, [x1, #-8]!
	stlr x0, [x1, #-4]!
	stlr w0, [x1, #4]!
	stlr x0, [x1, #8]!

	/* Invalid load pair transfer with register overlap */
	ldiapp w0, w0, [x1]
	ldiapp x0, x0, [x1]
	ldiapp w0, w0, [x1], #8
	ldiapp x0, x0, [x1], #16

	stilp w0, w0, [x1]
	stilp x0, x0, [x1]
	stilp w0, w0, [x1, #-8]!
	stilp x0, x0, [x1, #-16]!

	/* Invalid write back overlap (load)*/
	ldiapp x0, x1, [x0], #16
	ldiapp x0, x1, [x1], #16
	ldiapp w0, w1, [x0], #8
	ldiapp w0, w1, [x1], #8

	ldapr x0, [x0], #8
	ldapr w0, [x0], #4
	ldapr x1, [x1], #8
	ldapr x30, [x30], #8

	/* Invalid write back overlap (store)*/
	stilp x0, x1, [x1, #-16]!
	stilp w0, w1, [x1, #-8]!
	stilp x0, x1, [x0, #-16]!
	stilp w0, w1, [x0, #-8]!

	stlr x0, [x0, #-8]!
	stlr w0, [x0, #-4]!
	stlr x1, [x1, #-8]!
	stlr x30, [x30, #-8]!
