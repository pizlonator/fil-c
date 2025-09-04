.text
foo:

#		D1(B1),I2
	mvi	16(),32			# syntax error: empty parentheses

#		R1,D2(X2,B2)
	a	%r1,16(,)		# syntax error: explicitly omitted index & base
	a	%r1,16()		# syntax error: empty parentheses

#		D1(L1,B1),D2(B2)
	mvc	16(1,%r1),32()		# syntax error: empty parentheses
	mvc	16(1,%r0),32()		# syntax error: empty parentheses
	mvc	16(1,0),32()		# syntax error: empty parentheses
	mvc	16(1,),32()		# syntax error: explicitly omitted base & empty parentheses
	mvc	16(1),32()		# syntax error: empty parentheses
	mvc	16(0,%r1),32(%r2)	# syntax error: length 0
	mvc	16(,%r1),32(%r2)	# syntax error: explicitly omitted length
	mvc	16(,1),32(%r2)		# syntax error: explicitly omitted length
	mvc	16(,),32(%r2)		# syntax error: explicitly omitted length & base
	mvc	16(),32(%r2)		# syntax error: empty parenthesis
	mvc	16,32(%r2)		# syntax error: missing length
	mvc	16(%r1),32(%r2)		# syntax error: omitted length

#		D1(L1,B1),D2(L2,B2)
	unpk	16(0,%r1),32(2,%r2)	# syntax error: length 0
	unpk	16(,%r1),32(2,%r2)	# syntax error: explicitly omitted length
	unpk	16(,1),32(2,%r2)	# syntax error: explicitly omitted length
	unpk	16(,),32(2,%r2)		# syntax error: explicitly omitted length & base
	unpk	16(),32(2,%r2)		# syntax error: empty parenthesis
	unpk	16,32(2,%r2)		# syntax error: missing length
	unpk	16(1,%r1),32(0,%r2)	# syntax error: length 0
	unpk	16(1,%r1),32(,%r2)	# syntax error: explicitly omitted length
	unpk	16(1,%r1),32(,2)	# syntax error: explicitly omitted length
	unpk	16(1,%r1),32(,)		# syntax error: explicitly omitted length & base
	unpk	16(1,%r1),32()		# syntax error: empty parenthesis
	unpk	16(1,%r1),32		# syntax error: missing length
	unpk	16(0,%r1),32(0,%r2)	# syntax error: lengths 0
	unpk	16(,%r1),32(,%r2)	# syntax error: explicitly omitted length
	unpk	16(,1),32(,2)		# syntax error: explicitly omitted length
	unpk	16(,),32(,)		# syntax error: explicitly omitted length & base
	unpk	16(),32()		# syntax error: empty parentheses
	unpk	16,32			# syntax error: missing lengths

#		V1,D2(VX2,B2),M3
	vgef	%v1,16(,),0		# syntax error: explicitly omitted index & base
	vgef	%v1,16(),0		# syntax error: empty parentheses
