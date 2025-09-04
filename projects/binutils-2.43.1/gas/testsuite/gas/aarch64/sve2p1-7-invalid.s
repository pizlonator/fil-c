/* PMOV (to predicate).  */
pmov	p0, z0
pmov	p16.b, z0
pmov	p0.b, z31[1]
pmov	p15.b
pmov	p15, z31.b[4]
pmov	p7, z15.b
mov	p7, w15.b
mov	p7.b, x15

pmov	p0, z0[3]
pmov	p16.h, z0
pmov	p0.h, z31[2]
pmov	p15.h
pmov	p15, z31.h[4]
pmov	p7.h, z15.h
pmov	p7.h, x15.h
pmov	p7.h, w15

pmov	p0, z0[4]
pmov	p16.s, z0
pmov	p0.s, z31[5]
pmov	p15.s
pmov	p15, z31.s[6]
pmov	p7.s, z15.s
pmov	p7.s, w15.s
pmov	p7.s, x15

pmov	p0, z0[8]
pmov	p16.d, z0
pmov	p0.d, z31[10]
pmov	p15.d
pmov	p15, z31.d[12]
pmov	p7.d, z15.d
pmov	p7.d, x15.d
pmov	p7.d, w15

/* PMOV (to vector).  */
pmov	z0, p0
pmov	z0, p16.b
pmov	z31[1], p0.b
pmov	p15.b
pmov	z31.b[4], p15
pmov	z15.b, p7.b
pmov	x15.b, p7.b
pmov	w15.b, p7

pmov	z0[3], p0
pmov	z0, p16.h
pmov	z31[2], p0.h
pmov	p15.h
pmov	z31.h[5], p15
pmov	z15, p7
pmov	z8.h, p7.h
pmov	x8.h, p7.h
pmov	w8.h, p7

pmov	z0[5], p0.s
pmov	z0, p16.s
pmov	z31[6], p0.s
pmov	p15.s
pmov	z31.s[7], p15
pmov	z15, p7
pmov	z8.s, p7.s
pmov	x8.s, p7.s
pmov	w8.s, p7

pmov	z0[8], p0.d
pmov	z0, p16.d
pmov	z31[9], p0.d
pmov	p15.d
pmov	z31.d[10], p15
pmov	z15, p7
pmov	z8.d, p7.d
pmov	w8.d, p7.d
pmov	x8.d, p7
