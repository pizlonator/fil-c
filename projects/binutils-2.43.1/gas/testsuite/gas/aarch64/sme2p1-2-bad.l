.*: Assembler messages:
.*: Error: the register list must have a stride of 1 or 8 at operand 1 -- `luti2 {z0.b,z7.b},zt0,z0\[0\]`
.*: Error: the register list must have a stride of 1 at operand 1 -- `luti2 {Z0.s,Z8.s},ZT0,Z0\[0\]'
.*: Error: operand mismatch -- `luti2 {z7.d,z15.d},zt0,z0\[0\]'
.*: Info:    did you mean this\?
.*: Info:    	luti2 {z7.b, z15.b}, zt0, z0\[0\]
.*: Info:    other valid variant\(s\):
.*: Info:    	luti2 {z7.h, z15.h}, zt0, z0\[0\]
.*: Info:    	luti2 {z7.s, z15.s}, zt0, z0\[0\]
.*: Error: operand 3 must be an indexed SVE vector register -- `luti2 {z16.b,z24.b},zt0,z0'
.*: Error: register element index out of range 0 to 7 at operand 3 -- `luti2 {z23.b,z31.b},zt0,z0\[8\]'
.*: Error: register element index out of range 0 to 7 at operand 3 -- `luti2 {z0.b,z8.b},zt0,z31\[15\]'
.*: Error: unexpected register type at operand 2 -- `luti2 {z0.b,z8.b},z0\[7\]'
.*: Error: the register list must have a stride of 1 or 8 at operand 1 -- `luti2 {z8.b,z24.b},zt0,z31\[0\]`
.*: Error: invalid register list at operand 1 -- `luti2 {z24.b,z24.b},zt0,z0\[7\]'
.*: Error: the register list must have a stride of 1 or 8 at operand 1 -- `luti2 {z4.h,z16.h},zt0,z20\[4\]`
.*: Error: the register list must have a stride of 1 or 8 at operand 1 -- `luti2 {z20.h,z22.h},zt0,z12\[2\]`
.*: Error: invalid register list at operand 1 -- `luti2 {z0.b,z3.b,z18.b,z1.b},zt0,z0\[0\]'
.*: Error: the register list must have a stride of 1 at operand 1 -- `luti2 {Z0.s,Z4.s,Z8.s,Z12.s},ZT0,Z0\[0\]'
.*: Error: operand mismatch -- `luti2 {z3.d,z7.d,z11.d,z15.d},zt0,z0\[0\]'
.*: Info:    did you mean this\?
.*: Info:    	luti2 {z3.b, z7.b, z11.b, z15.b}, zt0, z0\[0\]
.*: Info:    other valid variant\(s\):
.*: Info:    	luti2 {z3.h, z7.h, z11.h, z15.h}, zt0, z0\[0\]
.*: Info:    	luti2 {z3.s, z7.s, z11.s, z15.s}, zt0, z0\[0\]
.*: Error: operand 3 must be an indexed SVE vector register -- `luti2 {z16.h,z20.h,z24.h,z28.h},zt0,z0'
.*: Error: register element index out of range 0 to 3 at operand 3 -- `luti2 {z19.h,z23.h,z27.h,z31.h},zt0,z0\[5\]'
.*: Error: start register out of range at operand 1 -- `luti2 {z10.b,z14.b,z18.b,z22.b},zt0,z31\[0\]'
.*: Error: invalid register list at operand 1 -- `luti2 {z20.b,z24.b,z28.b,z30.b},z0\[3\]'
.*: Error: invalid register list at operand 1 -- `luti2 {z4.b,z7.b,z11.b,z18.b},zt0,z31\[0\]'
.*: Error: type mismatch in vector register list at operand 1 -- `luti2 {z6.b,z0.s,z2.d,z28.h},zt0,z0\[7\]'
.*: Error: the register list must have a stride of 1 or 8 at operand 1 -- `luti4 {z0.b,z7.b},zt0,z0\[0\]`
.*: Error: the register list must have a stride of 1 at operand 1 -- `luti2 {Z0.s,Z8.s},ZT0,Z0\[0\]'
.*: Error: operand mismatch -- `luti4 {z7.d,z15.d},zt0,z0\[0\]'
.*: Info:    did you mean this\?
.*: Info:    	luti4 {z7.b, z15.b}, zt0, z0\[0\]
.*: Info:    other valid variant\(s\):
.*: Info:    	luti4 {z7.h, z15.h}, zt0, z0\[0\]
.*: Info:    	luti4 {z7.s, z15.s}, zt0, z0\[0\]
.*: Error: missing braces at operand 3 -- `luti4 {z16.b,z24.b},zt0,z0'
.*: Error: register element index out of range 0 to 3 at operand 3 -- `luti4 {z23.b,z31.b},zt0,z0\[8\]'
.*: Error: register element index out of range 0 to 3 at operand 3 -- `luti4 {z0.b,z8.b},zt0,z31\[15\]'
.*: Error: unexpected register type at operand 2 -- `luti4 {z0.b,z8.b},z0\[7\]'
.*: Error: the register list must have a stride of 1 or 8 at operand 1 -- `luti4 {z8.b,z24.b},zt0,z31\[0\]`
.*: Error: invalid register list at operand 1 -- `luti4 {z24.b,z24.b},zt0,z0\[7\]'
.*: Error: the register list must have a stride of 1 or 8 at operand 1 -- `luti4 {z4.h,z16.h},zt0,z20\[4\]`
.*: Error: the register list must have a stride of 1 or 8 at operand 1 -- `luti4 {z20.h,z22.h},zt0,z12\[2\]`
.*: Error: the register list must have a stride of 1 at operand 1 -- `luti4 {z0.s,z4.s,z8.s,z12.s},zt0,z0\[0\]'
.*: Error: invalid register list at operand 1 -- `luti4 {z0.b,z3.b,z18.b,z1.b},zt0,z0\[0\]'
.*: Error: the register list must have a stride of 1 at operand 1 -- `luti2 {Z0.s,Z4.s,Z8.s,Z12.s},ZT0,Z0\[0\]'
.*: Error: operand mismatch -- `luti4 {z3.d,z7.d,z11.d,z15.d},zt0,z0\[0\]'
.*: Info:    did you mean this\?
.*: Info:    	luti4 {z3.b, z7.b, z11.b, z15.b}, zt0, z0\[0\]
.*: Info:    other valid variant\(s\):
.*: Info:    	luti4 {z3.h, z7.h, z11.h, z15.h}, zt0, z0\[0\]
.*: Info:    	luti4 {z3.s, z7.s, z11.s, z15.s}, zt0, z0\[0\]
.*: Error: missing braces at operand 3 -- `luti4 {z16.h,z20.h,z24.h,z28.h},zt0,z0'
.*: Error: register element index out of range 0 to 1 at operand 3 -- `luti4 {z19.h,z23.h,z27.h,z31.h},zt0,z0\[5\]'
.*: Error: expected a list of 2 registers at operand 1 -- `luti4 {z10.b,z14.b,z18.b,z22.b},zt0,z31\[0\]'
.*: Error: invalid register list at operand 1 -- `luti4 {z20.b,z24.b,z28.b,z30.b},z0\[3\]'
.*: Error: invalid register list at operand 1 -- `luti4 {z4.b,z7.b,z11.b,z18.b},zt0,z31\[0\]'
.*: Error: type mismatch in vector register list at operand 1 -- `luti4 {z6.b,z0.s,z2.d,z28.h},zt0,z0\[7\]'
