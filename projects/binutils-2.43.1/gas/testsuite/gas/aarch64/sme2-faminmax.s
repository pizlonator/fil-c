// Valid instructions
famax	{ z0.h - z1.h }, { z0.h - z1.h }, { z0.h - z1.h }
famax	{ z30.h - z31.h }, { z30.h - z31.h }, { z0.h - z1.h }
famax	{ z0.h - z1.h }, { z0.h - z1.h }, { z30.h - z31.h }
famax	{ z18.h - z19.h }, { z18.h - z19.h }, { z26.h - z27.h }

famax	{ z0.s - z1.s }, { z0.s - z1.s }, { z0.s - z1.s }
famax	{ z30.s - z31.s }, { z30.s - z31.s }, { z0.s - z1.s }
famax	{ z0.s - z1.s }, { z0.s - z1.s }, { z30.s - z31.s }
famax	{ z4.s - z5.s }, { z4.s - z5.s }, { z10.s - z11.s }

famax	{ z0.d - z1.d }, { z0.d - z1.d }, { z0.d - z1.d }
famax	{ z30.d - z31.d }, { z30.d - z31.d }, { z0.d - z1.d }
famax	{ z0.d - z1.d }, { z0.d - z1.d }, { z30.d - z31.d }
famax	{ z28.d - z29.d }, { z28.d - z29.d }, { z8.d - z9.d }

famax	{ z0.h - z3.h }, { z0.h - z3.h }, { z0.h - z3.h }
famax	{ z28.h - z31.h }, { z28.h - z31.h }, { z0.h - z3.h }
famax	{ z0.h - z3.h }, { z0.h - z3.h }, { z28.h - z31.h }
famax	{ z16.h - z19.h }, { z16.h - z19.h }, { z20.h - z23.h }

famax	{ z0.s - z3.s }, { z0.s - z3.s }, { z0.s - z3.s }
famax	{ z28.s - z31.s }, { z28.s - z31.s }, { z0.s - z3.s }
famax	{ z0.s - z3.s }, { z0.s - z3.s }, { z28.s - z31.s }
famax	{ z24.s - z27.s }, { z24.s - z27.s }, { z4.s - z7.s }

famax	{ z0.d - z3.d }, { z0.d - z3.d }, { z0.d - z3.d }
famax	{ z28.d - z31.d }, { z28.d - z31.d }, { z0.d - z3.d }
famax	{ z0.d - z3.d }, { z0.d - z3.d }, { z28.d - z31.d }
famax	{ z12.d - z15.d }, { z12.d - z15.d }, { z8.d - z11.d }

// Invalid instruction because sz is 00 which is invalid
.inst	0xc128b94c

// Valid instructions
famin	{ z0.h - z1.h }, { z0.h - z1.h }, { z0.h - z1.h }
famin	{ z30.h - z31.h }, { z30.h - z31.h }, { z0.h - z1.h }
famin	{ z0.h - z1.h }, { z0.h - z1.h }, { z30.h - z31.h }
famin	{ z18.h - z19.h }, { z18.h - z19.h }, { z26.h - z27.h }

famin	{ z0.s - z1.s }, { z0.s - z1.s }, { z0.s - z1.s }
famin	{ z30.s - z31.s }, { z30.s - z31.s }, { z0.s - z1.s }
famin	{ z0.s - z1.s }, { z0.s - z1.s }, { z30.s - z31.s }
famin	{ z4.s - z5.s }, { z4.s - z5.s }, { z10.s - z11.s }

famin	{ z0.d - z1.d }, { z0.d - z1.d }, { z0.d - z1.d }
famin	{ z30.d - z31.d }, { z30.d - z31.d }, { z0.d - z1.d }
famin	{ z0.d - z1.d }, { z0.d - z1.d }, { z30.d - z31.d }
famin	{ z28.d - z29.d }, { z28.d - z29.d }, { z8.d - z9.d }

famin	{ z0.h - z3.h }, { z0.h - z3.h }, { z0.h - z3.h }
famin	{ z28.h - z31.h }, { z28.h - z31.h }, { z0.h - z3.h }
famin	{ z0.h - z3.h }, { z0.h - z3.h }, { z28.h - z31.h }
famin	{ z16.h - z19.h }, { z16.h - z19.h }, { z20.h - z23.h }

famin	{ z0.s - z3.s }, { z0.s - z3.s }, { z0.s - z3.s }
famin	{ z28.s - z31.s }, { z28.s - z31.s }, { z0.s - z3.s }
famin	{ z0.s - z3.s }, { z0.s - z3.s }, { z28.s - z31.s }
famin	{ z24.s - z27.s }, { z24.s - z27.s }, { z4.s - z7.s }

famin	{ z0.d - z3.d }, { z0.d - z3.d }, { z0.d - z3.d }
famin	{ z28.d - z31.d }, { z28.d - z31.d }, { z0.d - z3.d }
famin	{ z0.d - z3.d }, { z0.d - z3.d }, { z28.d - z31.d }
famin	{ z12.d - z15.d }, { z12.d - z15.d }, { z8.d - z11.d }

// Invalid instruction because sz is 00 which is invalid
.inst	0xc128B94d
