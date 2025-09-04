.*: Assembler messages:
.*: Error: operand mismatch -- `extq z0.b,z0.h,z0.b,#0'
.*: Info:    did you mean this\?
.*: Info:    	extq z0.b, z0.b, z0.b, #0
.*: Error: operand 2 must be the same register as operand 1 -- `extq z31.b,z15.b,z0.b,#0'
.*: Error: operand mismatch -- `extq z0.b,z0.b,z31.h,#0'
.*: Info:    did you mean this\?
.*: Info:    	extq z0.b, z0.b, z31.b, #0
.*: Error: immediate value out of range 0 to 15 at operand 4 -- `extq z0.b,z0.b,z0.b,#16'
.*: Error: operand mismatch -- `extq z0.h,z0.h,z0.h,#15'
.*: Info:    did you mean this\?
.*: Info:    	extq z0.b, z0.b, z0.b, #15
.*: Warning: output register of preceding `movprfx' not used in current instruction at operand 1 -- `extq z3.b,z3.b,z0.b,#0'
.*: Error: operand 2 must be the same register as operand 1 -- `extq z31.b,z2.b,z0.b,#15'
.*: Warning: instruction opens new dependency sequence without ending previous one -- `movprfx z31.b,p1/m,z10.b'
.*: Warning: predicated instruction expected after `movprfx' -- `extq z31.b,z31.b,z0.b,#15'
.*: Warning: output register of preceding `movprfx' used as input at operand 3 -- `extq z0.b,z0.b,z0.b,#0'
