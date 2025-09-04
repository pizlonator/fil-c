.*: Assembler messages:

.*: Error: operand mismatch -- `addpt z0.b,p0/m,z0.b,z0.b'
.*: Info:\s+did you mean this\?
.*: Info:\s+addpt z0.d, p0/m, z0.d, z0.d
.*: Error: operand 3 must be the same register as operand 1 -- `addpt z0.d,p0/m,z1.d,z0.d'
.*: Error: operand mismatch -- `addpt z0.d,p0/m,z0.d,z0.s'
.*: Info:\s+did you mean this\?
.*: Info:\s+addpt z0.d, p0/m, z0.d, z0.d
.*: Error: operand mismatch -- `addpt z0.h,p0/m,z0.h,z0.d'
.*: Info:\s+did you mean this\?
.*: Info:\s+addpt z0.d, p0/m, z0.d, z0.d
.*: Error: expected an integer or stack pointer register at operand 2 -- `addpt x1,p0/m,z0.d,z0.d'
.*: Error: expected an SVE vector or predicate register at operand 2 -- `addpt z0.d,x1,z0.d,z0.d'
.*: Error: expected an SVE vector register at operand 3 -- `addpt z0.d,p0/m,x1,z0.d'
.*: Error: expected an SVE vector register at operand 4 -- `addpt z0.d,p0/m,z0.d,x1'

.*: Error: operand mismatch -- `subpt z0.b,p0/m,z0.b,z0.b'
.*: Info:\s+did you mean this\?
.*: Info:\s+subpt z0.d, p0/m, z0.d, z0.d
.*: Error: operand 3 must be the same register as operand 1 -- `subpt z0.d,p0/m,z1.d,z0.d'
.*: Error: operand mismatch -- `subpt z0.d,p0/m,z0.d,z0.s'
.*: Info:\s+did you mean this\?
.*: Info:\s+subpt z0.d, p0/m, z0.d, z0.d
.*: Error: operand mismatch -- `subpt z0.h,p0/m,z0.h,z0.d'
.*: Info:\s+did you mean this\?
.*: Info:\s+subpt z0.d, p0/m, z0.d, z0.d
.*: Error: expected an integer or stack pointer register at operand 2 -- `subpt x1,p0/m,z0.d,z0.d'
.*: Error: expected an SVE vector or predicate register at operand 2 -- `subpt z0.d,x1,z0.d,z0.d'
.*: Error: expected an SVE vector register at operand 3 -- `subpt z0.d,p0/m,x1,z0.d'
.*: Error: expected an SVE vector register at operand 4 -- `subpt z0.d,p0/m,z0.d,x1'

.*: Error: operand mismatch -- `addpt z0.b,z0.b,z0.b'
.*: Info:\s+did you mean this\?
.*: Info:\s+addpt z0.d, z0.d, z0.d
.*: Error: operand mismatch -- `addpt z0.h,z0.h,z0.h'
.*: Info:\s+did you mean this\?
.*: Info:\s+addpt z0.d, z0.d, z0.d
.*: Error: operand mismatch -- `addpt z0.s,z0.s,z0.s'
.*: Info:\s+did you mean this\?
.*: Info:\s+addpt z0.d, z0.d, z0.d
.*: Error: expected an integer or stack pointer register at operand 2 -- `addpt x15,z0.d,z0.d'
.*: Error: expected an SVE vector or predicate register at operand 2 -- `addpt z0.d,x15,z0.d'
.*: Error: expected an SVE vector register at operand 3 -- `addpt z0.d,z0.d,x15'

.*: Error: operand mismatch -- `subpt z0.b,z0.b,z0.b'
.*: Info:\s+did you mean this\?
.*: Info:\s+subpt z0.d, z0.d, z0.d
.*: Error: operand mismatch -- `subpt z0.h,z0.h,z0.h'
.*: Info:\s+did you mean this\?
.*: Info:\s+subpt z0.d, z0.d, z0.d
.*: Error: operand mismatch -- `subpt z0.s,z0.s,z0.s'
.*: Info:\s+did you mean this\?
.*: Info:\s+subpt z0.d, z0.d, z0.d
.*: Error: expected an integer or stack pointer register at operand 2 -- `subpt x9,z0.d,z0.d'
.*: Error: expected an SVE vector or predicate register at operand 2 -- `subpt z0.d,x9,z0.d'
.*: Error: expected an SVE vector register at operand 3 -- `subpt z0.d,z0.d,x9'

.*: Error: expected an SVE vector register at operand 2 -- `madpt z0.d,p0/m,z0.d,z0.d'
.*: Error: operand mismatch -- `madpt z10.b,z20.b,z30.b'
.*: Info:\s+did you mean this\?
.*: Info:\s+madpt z10.d, z20.d, z30.d
.*: Error: operand mismatch -- `madpt z20.h,z20.h,z30.h'
.*: Info:\s+did you mean this\?
.*: Info:\s+madpt z20.d, z20.d, z30.d
.*: Error: operand mismatch -- `madpt z20.s,z20.s,z30.s'
.*: Info:\s+did you mean this\?
.*: Info:\s+madpt z20.d, z20.d, z30.d
.*: Error: expected an SVE vector register at operand 1 -- `madpt w9,z0.d,z0.d'
.*: Error: expected an SVE vector register at operand 2 -- `madpt z0.d,w9,z0.d'
.*: Error: expected an SVE vector register at operand 3 -- `madpt z0.d,z0.d,w9'

.*: Error: expected an SVE vector register at operand 2 -- `mlapt z0.d,p3/m,z0.d,z0.d'
.*: Error: operand mismatch -- `mlapt z10.b,z20.b,z30.b'
.*: Info:\s+did you mean this\?
.*: Info:\s+mlapt z10.d, z20.d, z30.d
.*: Error: operand mismatch -- `mlapt z20.h,z20.h,z30.h'
.*: Info:\s+did you mean this\?
.*: Info:\s+mlapt z20.d, z20.d, z30.d
.*: Error: operand mismatch -- `mlapt z20.s,z20.s,z30.s'
.*: Info:\s+did you mean this\?
.*: Info:\s+mlapt z20.d, z20.d, z30.d
.*: Error: expected an SVE vector register at operand 1 -- `mlapt w7,z0.d,z0.d'
.*: Error: expected an SVE vector register at operand 2 -- `mlapt z0.d,w7,z0.d'
.*: Error: expected an SVE vector register at operand 3 -- `mlapt z0.d,z0.d,w7'

.*: Warning: output register of preceding `movprfx' expected as output at operand 1 -- `addpt z1.d,p0/m,z1.d,z2.d'
.*: Warning: predicate register differs from that in preceding `movprfx' at operand 2 -- `addpt z1.d,p3/m,z1.d,z2.d'
.*: Warning: output register of preceding `movprfx' expected as output at operand 1 -- `subpt z1.d,p0/m,z1.d,z2.d'
.*: Warning: predicate register differs from that in preceding `movprfx' at operand 2 -- `subpt z1.d,p3/m,z1.d,z2.d'
.*: Warning: predicated instruction expected after `movprfx' -- `madpt z10.d,z20.d,z30.d'
.*: Warning: predicated instruction expected after `movprfx' -- `mlapt z10.d,z20.d,z30.d'
