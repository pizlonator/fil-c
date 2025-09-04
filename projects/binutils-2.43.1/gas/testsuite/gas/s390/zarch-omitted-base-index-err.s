.text
foo:

#		R1,D2(B2)
	clm	%r1,0b1000,16()

#		R1,D2(X2,B2)
	a	%r1,16()

#		V1,D2(VX2,B2),M3
	vgef	%v1,16(%v2),0
	vgef	%v1,16(),0

#		D1(L1,B1),D2(B2)
	mvc	16(,%r1),32(%r2)
	mvc	16(,1),32(%r2)
	mvc	16(%r1),32(%r2)
	mvc	16(),32(%r2)
	mvc	16,32(%r2)
