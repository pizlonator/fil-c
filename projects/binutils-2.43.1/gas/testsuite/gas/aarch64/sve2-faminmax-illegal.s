// Operand mismatch
famax	z5.h, p0/m, z5.h, z7.s
famax	z5.s, p2/m, z5.s, z7.h
famin	z5.h, p0/m, z5.h, z7.s
famin	z5.s, p2/m, z5.s, z7.h

// Incorrect operands
famax	z5.h, p0/m, z7.h, z9.h
famin	z5.h, p0/m, z7.h, z9.h

// Disallowed types
famax	z5.b, p2/m, z5.b, z5.b
famin	z5.b, p2/m, z5.b, z5.b

// Incorrect number of arguments
famax	z5.h
famax	z5.h, p2/m
famax	z5.h, p2/m, z5.h
famax	z5.h, p2/m, z5.h, z6.h, z6.h
famin	z5.h
famin	z5.h, p2/m
famin	z5.h, p2/m, z5.h
famin	z5.h, p2/m, z5.h, z6.h, z6.h

// Spelling mistakes
famax	z5.t, p2/m, z5.s, z7.s
famax	z5.s, p2/m, z5.t, z7.s
famax	z5.s, p2/m, z5.s, z7.t
famax	z5.s, j2/m, z5.s, z7.s
famin	z5.t, p2/m, z5.s, z7.s
famin	z5.s, p2/m, z5.t, z7.s
famin	z5.s, p2/m, z5.s, z7.t
famin	z5.s, j2/m, z5.s, z7.s

// Missing qualifiers
famax	5.h, p2/m, z5.h, z7.h
famax	z5.h, p2/m, 5.h, z7.h
famax	z5.h, p2/m, z5.h, 7.h
famax	z5.h, 2/m, z5.h, z7.h
famax	z5, p2/m, z5.h, z7.h
famax	z5.h, p2/m, z5, z7.h
famax	z5.h, p2/m, z5.h, z7
famax	z5.h, p2, z5.h, z7.h
famin	5.h, p2/m, z5.h, z7.h
famin	z5.h, p2/m, 5.h, z7.h
famin	z5.h, p2/m, z5.h, 7.h
famin	z5.h, 2/m, z5.h, z7.h
famin	z5, p2/m, z5.h, z7.h
famin	z5.h, p2/m, z5, z7.h
famin	z5.h, p2/m, z5.h, z7
famin	z5.h, p2, z5.h, z7.h

// Out of range numbers
famax	z35.s, p2/m, z5.s, z7.s
famax	z5.s, p2/m, z35.s, z7.s
famax	z5.s, p2/m, z5.s, z35.s
famax	z5.s, p9/m, z5.s, z7.s
famin	z35.s, p2/m, z5.s, z7.s
famin	z5.s, p2/m, z35.s, z7.s
famin	z5.s, p2/m, z5.s, z35.s
famin	z5.s, p9/m, z5.s, z7.s

// movprfx mismatch
movprfx    z6, z9
famax	   z5.h, p7/m, z5.h, z7.h
movprfx	   z5.d, p6/m, z8.d
famax	   z5.d, p7/m, z5.d, z9.d
movprfx    z6, z9
famin	   z5.h, p7/m, z5.h, z7.h
movprfx	   z5.d, p6/m, z8.d
famin	   z5.d, p7/m, z5.d, z9.d
