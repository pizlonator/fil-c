/* adcs reads the carry-in from NZCV (set here by the program's cmp) and adds
   it into its result, so the instrumented ldr between the cmp and the adcs
   must be bracketed by mrs/msr nzcv; otherwise the access check's compare
   clobbers CF and adcs adds the wrong carry. This is the LOAD-BEARING
   flag-liveness test for the adc/sbc family: removing adcs/sbcs from arm64
   FLAG_USE drops the bracket and f computes x+y (+*p+1) instead of
   x+y+1 (+*p+1) for x >= y. (See sarcasm-fccmp-arm for the structurally
   vacuous sibling: fccmp's NZCV output is only observable through a consumer
   that is itself a flagUse.) */
	.text
	.global	f
	.type	f, %function
f:                              ;! long(ptr, long, long)
	cmp	x1, x2
	ldr	x3, [x0]
	adcs	x1, x1, x2
	adds	x3, x3, #1
	add	x0, x1, x3
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
