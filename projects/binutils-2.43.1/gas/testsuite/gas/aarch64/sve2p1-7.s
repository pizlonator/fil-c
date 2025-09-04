/* PMOV (to predicate).  */
pmov	p0.b, z0
pmov	p15.b, z0
pmov	p0.b, z31
pmov	p15.b, z31
pmov	p15.b, z31[0]
pmov	p7.b, z15

pmov	p0.h, z0[0]
pmov	p15.h, z0[0]
pmov	p0.h, z31[0]
pmov	p0.h, z0[1]
pmov	p15.h, z31[1]
pmov	p7.h, z15[0]
pmov	p3.h, z8

pmov	p0.s, z0[0]
pmov	p15.s, z0[0]
pmov	p0.s, z31[0]
pmov	p0.s, z0[3]
pmov	p15.s, z31[3]
pmov	p7.s, z15[1]
pmov	p3.s, z8

pmov	p0.d, z0[0]
pmov	p15.d, z0[0]
pmov	p0.d, z31[0]
pmov	p0.d, z0[7]
pmov	p15.d, z31[7]
pmov	p7.d, z15[3]
pmov	p3.d, z8

/* PMOV (to vector).  */
pmov	z0, p0.b
pmov	z31, p0.b
pmov	z0, p15.b
pmov	z31, p15.b
pmov	z0[0], p7.b
pmov	z15[0], p7.b

pmov	z0[0], p0.h
pmov	z31[0], p0.h
pmov	z0[1], p0.h
pmov	z0[0], p15.h
pmov	z31[1], p15.h
pmov	z15[0], p15.h
pmov	z8, p7.h

pmov	z0[0], p0.s
pmov	z31[0], p0.s
pmov	z0[3], p0.s
pmov	z0[0], p15.s
pmov	z31[3], p15.s
pmov	z15[2], p15.s
pmov	z8, p7.s

pmov	z0[0], p0.d
pmov	z31[0], p0.d
pmov	z0[7], p0.d
pmov	z0[0], p15.d
pmov	z31[7], p15.d
pmov	z15[3], p15.d
pmov	z8, p7.d
