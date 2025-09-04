.*: Assembler messages:
.*: Error: operand mismatch -- `tblq z0.s,{z0.b},z0.b'
.*: Info:    did you mean this\?
.*: Info:    	tblq z0.b, {z0.b}, z0.b
.*: Info:    other valid variant\(s\):
.*: Info:    	tblq z0.h, {z0.h}, z0.h
.*: Info:    	tblq z0.s, {z0.s}, z0.s
.*: Info:    	tblq z0.d, {z0.d}, z0.d
.*: Error: expected a single-register list at operand 2 -- `tblq z0.s,{z0.s,z1.s},z0.s'
.*: Error: expected a single-register list at operand 2 -- `tblq z0.s,{z0.s-z1.s},z0.s'
.*: Error: operand mismatch -- `tblq z0.s,{z31.s},z0.b'
.*: Info:    did you mean this\?
.*: Info:    	tblq z0.s, {z31.s}, z0.s
.*: Info:    other valid variant\(s\):
.*: Info:    	tblq z0.b, {z31.b}, z0.b
.*: Info:    	tblq z0.h, {z31.h}, z0.h
.*: Info:    	tblq z0.d, {z31.d}, z0.d
.*: Error: expected an SVE vector register at operand 3 -- `tblq z0.b,{z0.b},{z31.b}'
.*: Error: expected an SVE vector register at operand 2 -- `tbxq z0.s,{z0.b},z0.b'
.*: Error: operand mismatch -- `tbxq z31.s,z0.b,z0.h'
.*: Info:    did you mean this\?
.*: Info:    	tbxq z31.b, z0.b, z0.b
.*: Info:    other valid variant\(s\):
.*: Info:    	tbxq z31.h, z0.h, z0.h
.*: Info:    	tbxq z31.s, z0.s, z0.s
.*: Info:    	tbxq z31.d, z0.d, z0.d
.*: Error: expected an SVE vector register at operand 2 -- `tbxq z0.s,{z0.s,z1.s},z0.s'
.*: Error: expected an SVE vector register at operand 2 -- `tbxq z0.h,{z0.h-z1.h},z0.h'
.*: Error: expected an SVE vector register at operand 1 -- `tbxq {z0.s},z31.s,z0.b'
.*: Error: expected an SVE vector register at operand 2 -- `tbxq z0.b,{z0.b},{z31.b}'
.*: Error: expected an SVE vector register at operand 2 -- `uzpq1 z0.s,{z0.b},z0.b'
.*: Error: operand mismatch -- `uzpq1 z31.s,z0.b,z0.h'
.*: Info:    did you mean this\?
.*: Info:    	uzpq1 z31.b, z0.b, z0.b
.*: Info:    other valid variant\(s\):
.*: Info:    	uzpq1 z31.h, z0.h, z0.h
.*: Info:    	uzpq1 z31.s, z0.s, z0.s
.*: Info:    	uzpq1 z31.d, z0.d, z0.d
.*: Error: expected an SVE vector register at operand 2 -- `uzpq1 z0.s,{z0.s,z1.s},z0.s'
.*: Error: expected an SVE vector register at operand 2 -- `uzpq1 z0.h,{z0.h-z1.h},z0.h'
.*: Error: expected an SVE vector register at operand 1 -- `uzpq1 {z0.s},z31.s,z0.b'
.*: Error: expected an SVE vector register at operand 2 -- `uzpq1 z0.b,{z0.b},{z31.b}'
.*: Error: expected an SVE vector register at operand 2 -- `uzpq2 z0.s,{z0.b},z0.b'
.*: Error: operand mismatch -- `uzpq2 z31.s,z0.b,z0.h'
.*: Info:    did you mean this\?
.*: Info:    	uzpq2 z31.b, z0.b, z0.b
.*: Info:    other valid variant\(s\):
.*: Info:    	uzpq2 z31.h, z0.h, z0.h
.*: Info:    	uzpq2 z31.s, z0.s, z0.s
.*: Info:    	uzpq2 z31.d, z0.d, z0.d
.*: Error: expected an SVE vector register at operand 2 -- `uzpq2 z0.s,{z0.s,z1.s},z0.s'
.*: Error: expected an SVE vector register at operand 2 -- `uzpq2 z0.h,{z0.h-z1.h},z0.h'
.*: Error: expected an SVE vector register at operand 1 -- `uzpq2 {z0.s},z31.s,z0.b'
.*: Error: expected an SVE vector register at operand 2 -- `uzpq2 z0.b,{z0.b},{z31.b}'
.*: Error: expected an SVE vector register at operand 2 -- `zipq1 z0.s,{z0.b},z0.b'
.*: Error: operand mismatch -- `zipq1 z31.s,z0.b,z0.h'
.*: Info:    did you mean this\?
.*: Info:    	zipq1 z31.b, z0.b, z0.b
.*: Info:    other valid variant\(s\):
.*: Info:    	zipq1 z31.h, z0.h, z0.h
.*: Info:    	zipq1 z31.s, z0.s, z0.s
.*: Info:    	zipq1 z31.d, z0.d, z0.d
.*: Error: expected an SVE vector register at operand 2 -- `zipq1 z0.s,{z0.s,z1.s},z0.s'
.*: Error: expected an SVE vector register at operand 2 -- `zipq1 z0.h,{z0.b-z1.h},z0.h'
.*: Error: expected an SVE vector register at operand 1 -- `zipq1 {z0.s},z31.s,z0.b'
.*: Error: expected an SVE vector register at operand 2 -- `zipq1 z0.b,{z0.b},{z31.b}'
.*: Error: expected an SVE vector register at operand 2 -- `zipq2 z0.s,{z0.b},z0.b'
.*: Error: operand mismatch -- `zipq2 z31.s,z0.b,z0.h'
.*: Info:    did you mean this\?
.*: Info:    	zipq2 z31.b, z0.b, z0.b
.*: Info:    other valid variant\(s\):
.*: Info:    	zipq2 z31.h, z0.h, z0.h
.*: Info:    	zipq2 z31.s, z0.s, z0.s
.*: Info:    	zipq2 z31.d, z0.d, z0.d
.*: Error: expected an SVE vector register at operand 2 -- `zipq2 z0.s,{z0.s,z1.s},z0.s'
.*: Error: expected an SVE vector register at operand 2 -- `zipq2 z0.h,{z0.b-z1.h},z0.h'
.*: Error: expected an SVE vector register at operand 1 -- `zipq2 {z0.s},z31.s,z0.b'
.*: Error: expected an SVE vector register at operand 2 -- `zipq2 z0.b,{z0.b},{z31.b}'
