.*: Assembler messages:
.*: Error: operand mismatch -- `pmov p0,z0'
.*: Info:    did you mean this\?
.*: Info:    	pmov p0.h, z0
.*: Error: expected an SVE vector or predicate register at operand 1 -- `pmov p16.b,z0'
.*: Error: register element index must be 0 at operand 2 -- `pmov p0.b,z31\[1\]'
.*: Error: comma expected between operands at operand 2 -- `pmov p15.b'
.*: Error: operand mismatch -- `pmov p15,z31.b\[4\]'
.*: Info:    did you mean this\?
.*: Info:    	pmov p15.h, z31\[4\]
.*: Error: operand mismatch -- `pmov p7,z15.b'
.*: Info:    did you mean this\?
.*: Info:    	pmov p7.h, z15
.*: Error: expected an SVE predicate register at operand 2 -- `mov p7,w15.b'
.*: Error: expected an SVE predicate register at operand 2 -- `mov p7.b,x15'
.*: Error: operand mismatch -- `pmov p0,z0\[3\]'
.*: Info:    did you mean this\?
.*: Info:    	pmov p0.h, z0\[3\]
.*: Error: expected an SVE vector or predicate register at operand 1 -- `pmov p16.h,z0'
.*: Error: register element index out of range 0 to 1 at operand 2 -- `pmov p0.h,z31\[2\]'
.*: Error: comma expected between operands at operand 2 -- `pmov p15.h'
.*: Error: operand mismatch -- `pmov p15,z31.h\[4\]'
.*: Info:    did you mean this\?
.*: Info:    	pmov p15.h, z31\[4\]
.*: Error: operand mismatch -- `pmov p7.h,z15.h'
.*: Info:    did you mean this\?
.*: Info:    	pmov p7.h, z15
.*: Error: expected an SVE vector register at operand 2 -- `pmov p7.h,x15.h'
.*: Error: expected an SVE vector register at operand 2 -- `pmov p7.h,w15'
.*: Error: operand mismatch -- `pmov p0,z0\[4\]'
.*: Info:    did you mean this\?
.*: Info:    	pmov p0.h, z0\[4\]
.*: Error: expected an SVE vector or predicate register at operand 1 -- `pmov p16.s,z0'
.*: Error: register element index out of range 0 to 3 at operand 2 -- `pmov p0.s,z31\[5\]'
.*: Error: comma expected between operands at operand 2 -- `pmov p15.s'
.*: Error: operand mismatch -- `pmov p15,z31.s\[6\]'
.*: Info:    did you mean this\?
.*: Info:    	pmov p15.h, z31\[6\]
.*: Error: operand mismatch -- `pmov p7.s,z15.s'
.*: Info:    did you mean this\?
.*: Info:    	pmov p7.s, z15
.*: Error: expected an SVE vector register at operand 2 -- `pmov p7.s,w15.s'
.*: Error: expected an SVE vector register at operand 2 -- `pmov p7.s,x15'
.*: Error: operand mismatch -- `pmov p0,z0\[8\]'
.*: Info:    did you mean this\?
.*: Info:    	pmov p0.h, z0\[8\]
.*: Error: expected an SVE vector or predicate register at operand 1 -- `pmov p16.d,z0'
.*: Error: register element index out of range 0 to 7 at operand 2 -- `pmov p0.d,z31\[10\]'
.*: Error: comma expected between operands at operand 2 -- `pmov p15.d'
.*: Error: operand mismatch -- `pmov p15,z31.d\[12\]'
.*: Info:    did you mean this\?
.*: Info:    	pmov p15.h, z31\[12\]
.*: Error: operand mismatch -- `pmov p7.d,z15.d'
.*: Info:    did you mean this\?
.*: Info:    	pmov p7.d, z15
.*: Error: expected an SVE vector register at operand 2 -- `pmov p7.d,x15.d'
.*: Error: expected an SVE vector register at operand 2 -- `pmov p7.d,w15'
.*: Error: operand mismatch -- `pmov z0,p0'
.*: Info:    did you mean this\?
.*: Info:    	pmov z0, p0.b
.*: Error: expected an SVE predicate register at operand 2 -- `pmov z0,p16.b'
.*: Error: register element index must be 0 at operand 1 -- `pmov z31\[1\],p0.b'
.*: Error: comma expected between operands at operand 2 -- `pmov p15.b'
.*: Error: operand mismatch -- `pmov z31.b\[4\],p15'
.*: Info:    did you mean this\?
.*: Info:    	pmov z31\[4\], p15.b
.*: Error: operand mismatch -- `pmov z15.b,p7.b'
.*: Info:    did you mean this\?
.*: Info:    	pmov z15, p7.b
.*: Error: expected an SVE vector or predicate register at operand 1 -- `pmov x15.b,p7.b'
.*: Error: expected an SVE vector or predicate register at operand 1 -- `pmov w15.b,p7'
.*: Error: operand mismatch -- `pmov z0\[3\],p0'
.*: Info:    did you mean this\?
.*: Info:    	pmov z0\[3\], p0.b
.*: Error: expected an SVE predicate register at operand 2 -- `pmov z0,p16.h'
.*: Error: register element index out of range 0 to 1 at operand 1 -- `pmov z31\[2\],p0.h'
.*: Error: comma expected between operands at operand 2 -- `pmov p15.h'
.*: Error: operand mismatch -- `pmov z31.h\[5\],p15'
.*: Info:    did you mean this\?
.*: Info:    	pmov z31\[5\], p15.b
.*: Error: operand mismatch -- `pmov z15,p7'
.*: Info:    did you mean this\?
.*: Info:    	pmov z15, p7.b
.*: Error: operand mismatch -- `pmov z8.h,p7.h'
.*: Info:    did you mean this\?
.*: Info:    	pmov z8, p7.h
.*: Error: expected an SVE vector or predicate register at operand 1 -- `pmov x8.h,p7.h'
.*: Error: expected an SVE vector or predicate register at operand 1 -- `pmov w8.h,p7'
.*: Error: register element index out of range 0 to 3 at operand 1 -- `pmov z0\[5\],p0.s'
.*: Error: expected an SVE predicate register at operand 2 -- `pmov z0,p16.s'
.*: Error: register element index out of range 0 to 3 at operand 1 -- `pmov z31\[6\],p0.s'
.*: Error: comma expected between operands at operand 2 -- `pmov p15.s'
.*: Error: operand mismatch -- `pmov z31.s\[7\],p15'
.*: Info:    did you mean this\?
.*: Info:    	pmov z31\[7\], p15.b
.*: Error: operand mismatch -- `pmov z15,p7'
.*: Info:    did you mean this\?
.*: Info:    	pmov z15, p7.b
.*: Error: operand mismatch -- `pmov z8.s,p7.s'
.*: Info:    did you mean this\?
.*: Info:    	pmov z8, p7.s
.*: Error: expected an SVE vector or predicate register at operand 1 -- `pmov x8.s,p7.s'
.*: Error: expected an SVE vector or predicate register at operand 1 -- `pmov w8.s,p7'
.*: Error: register element index out of range 0 to 7 at operand 1 -- `pmov z0\[8\],p0.d'
.*: Error: expected an SVE predicate register at operand 2 -- `pmov z0,p16.d'
.*: Error: register element index out of range 0 to 7 at operand 1 -- `pmov z31\[9\],p0.d'
.*: Error: comma expected between operands at operand 2 -- `pmov p15.d'
.*: Error: operand mismatch -- `pmov z31.d\[10\],p15'
.*: Info:    did you mean this\?
.*: Info:    	pmov z31\[10\], p15.b
.*: Error: operand mismatch -- `pmov z15,p7'
.*: Info:    did you mean this\?
.*: Info:    	pmov z15, p7.b
.*: Error: operand mismatch -- `pmov z8.d,p7.d'
.*: Info:    did you mean this\?
.*: Info:    	pmov z8, p7.d
.*: Error: expected an SVE vector or predicate register at operand 1 -- `pmov w8.d,p7.d'
.*: Error: expected an SVE vector or predicate register at operand 1 -- `pmov x8.d,p7'
