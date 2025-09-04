.text
foo:

#		D1(B1),I2
	mvi	16(%r1),32
	mvi	16(%r0),32
	mvi	16(0),32
#	mvi	16(),32			# syntax error: empty parentheses
	mvi	16,32
	mvi	0,32
	mvi	0,0

#		R1,D2(X2,B2)
	a	%r1,16(%r2,%r3)
	a	%r1,16(%r0,%r3)
	a	%r1,16(0,%r3)
	a	%r1,16(,%r3)
	a	%r1,16(%r3)
	a	%r1,16(%r2,%r0)
	a	%r1,16(%r2,0)
#	a	%r1,16(%r2,)		# syntax error: explicitly omitted base
	a	%r1,16(%r0,%r0)
	a	%r1,16(%r0,0)
#	a	%r1,16(%r0,)		# syntax error: explicitly omitted base
	a	%r1,16(0,%r0)
	a	%r1,16(0,0)
#	a	%r1,16(0,)		# syntax error: explicitly omitted base
	a	%r1,16(0,%r0)
	a	%r1,16(,%r0)
	a	%r1,16(,0)
#	a	%r1,16(,)		# syntax error: explicitly omitted index & base
#	a	%r1,16()		# syntax error: empty parentheses
	a	%r1,16
	a	%r1,0
	a	%r0,0
	a	0,0

#		D1(L1,B1),D2(B2)
	mvc	16(1,%r1),32(%r2)
	mvc	16(1,%r0),32(%r2)
	mvc	16(1,0),32(%r2)
#	mvc	16(1,),32(%r2)		# syntax error: explicitly omitted base
	mvc	16(1),32(%r2)
	mvc	16(1,%r1),32(%r0)
	mvc	16(1,%r1),32(0)
#	mvc	16(1,%r1),32()		# syntax error: empty parentheses
	mvc	16(1,%r1),32
	mvc	16(1,%r0),32(%r0)
	mvc	16(1,%r0),32(0)
#	mvc	16(1,%r0),32()		# syntax error: empty parentheses
	mvc	16(1,%r0),32
	mvc	16(1,0),32(%r0)
	mvc	16(1,0),32(0)
#	mvc	16(1,0),32()		# syntax error: empty parentheses
	mvc	16(1,0),32
#	mvc	16(1,),32(%r0)		# syntax error: explicitly omitted base
#	mvc	16(1,),32(0)		# syntax error: explicitly omitted base
#	mvc	16(1,),32()		# syntax error: explicitly omitted base & empty parentheses
#	mvc	16(1,),32		# syntax error: explicitly omitted base
	mvc	16(1),32(%r0)
	mvc	16(1),32(0)
#	mvc	16(1),32()		# syntax error: empty parentheses
	mvc	16(1),32
	mvc	0(1),0

#		D1(L1,B1),D2(L2,B2)
	unpk	16(1,%r1),32(2,%r2)
	unpk	16(1,%r0),32(2,%r2)
	unpk	16(1,0),32(2,%r2)
#	unpk	16(1,),32(2,%r2)	# syntax error: explicitly omitted base
	unpk	16(1),32(2,%r2)
	unpk	16(1,%r1),32(2,%r0)
	unpk	16(1,%r1),32(2,0)
#	unpk	16(1,%r1),32(2,)	# syntax error: explicitly omitted base
	unpk	16(1,%r1),32(2)
	unpk	16(1,%r0),32(2,%r0)
	unpk	16(1,%r0),32(2,0)
#	unpk	16(1,%r0),32(2,)	# syntax error: explicitly omitted base
	unpk	16(1,%r0),32(2)
	unpk	16(1,0),32(2,%r0)
	unpk	16(1,0),32(2,0)
#	unpk	16(1,0),32(2,)		# syntax error: explicitly omitted base
	unpk	16(1,0),32(2)
#	unpk	16(1,),32(2,%r0)	# syntax error: explicitly omitted base
#	unpk	16(1,),32(2,0)		# syntax error: explicitly omitted base
#	unpk	16(1,),32(2,)		# syntax error: explicitly omitted base
#	unpk	16(1,),32(2)		# syntax error: explicitly omitted base
	unpk	16(1),32(2,%r0)
	unpk	16(1),32(2,0)
#	unpk	16(1),32(2,)		# syntax error: explicitly omitted base
	unpk	16(1),32(2)
	unpk	0(1),0(2)

#		V1,D2(VX2,B2),M3
	vgef	%v1,16(%v2,%r3),0
	vgef	%v1,16(%v0,%r3),0
	vgef	%v1,16(0,%r3),0
	vgef	%v1,16(,%r3),0
	vgef	%v1,16(%r3),0
	vgef	%v1,16(%v2,%r0),0
	vgef	%v1,16(%v2,0),0
#	vgef	%v1,16(%v2,),0		# syntax error: explicitly omitted base
	vgef	%v1,16(%v0,%r0),0
	vgef	%v1,16(%v0,0),0
#	vgef	%v1,16(%v0,),0		# syntax error: explicitly omitted base
	vgef	%v1,16(0,%r0),0
	vgef	%v1,16(0,0),0
#	vgef	%v1,16(0,),0		# syntax error: explicitly omitted base
	vgef	%v1,16(,%r0),0
	vgef	%v1,16(,0),0
#	vgef	%v1,16(,),0		# syntax error: explicitly omitted index & base
#	vgef	%v1,16(),0		# syntax error: empty parentheses
	vgef	%v1,16,0
	vgef	%v0,16,0
	vgef	0,16,0
	vgef	0,0,0
