.*: Assembler messages:
.*: Error: operand mismatch -- `addpt w5,w8,w0'
.*: Info:\s+did you mean this\?
.*: Info:\s+addpt x5, x8, x0
.*: Error: only 'LSL' shift is permitted at operand 3 -- `addpt x5,x8,x0,asr#6'
.*: Error: shift amount out of range 0 to 7 at operand 3 -- `addpt x5,x8,x0,lsl#9'
.*: Error: expected an integer or zero register at operand 3 -- `addpt x5,x8,sp,lsl#5'
.*: Error: unexpected register type at operand 1 -- `addpt xzr,x8,x0,lsl#3'

.*: Error: operand mismatch -- `subpt w5,w8,w0'
.*: Info:\s+did you mean this\?
.*: Info:\s+subpt x5, x8, x0
.*: Error: only 'LSL' shift is permitted at operand 3 -- `subpt x5,x8,x0,asr#6'
.*: Error: shift amount out of range 0 to 7 at operand 3 -- `subpt x5,x8,x0,lsl#9'
.*: Error: expected an integer or zero register at operand 3 -- `subpt x5,x8,sp,lsl#5'
.*: Error: unexpected register type at operand 1 -- `subpt xzr,x8,x0,lsl#3'

.*: Error: operand mismatch -- `maddpt w1,x2,x3,x4'
.*: Info:\s+did you mean this\?
.*: Info:\s+maddpt x1, x2, x3, x4
.*: Error: operand mismatch -- `maddpt x1,w2,x3,x4'
.*: Info:\s+did you mean this\?
.*: Info:\s+maddpt x1, x2, x3, x4
.*: Error: operand mismatch -- `maddpt x1,x2,w3,x4'
.*: Info:\s+did you mean this\?
.*: Info:\s+maddpt x1, x2, x3, x4
.*: Error: operand mismatch -- `maddpt x1,x2,x3,w4'
.*: Info:\s+did you mean this\?
.*: Info:\s+maddpt x1, x2, x3, x4
.*: Error: expected an integer or zero register at operand 1 -- `maddpt sp,x2,x3,x4'
.*: Error: expected an integer or zero register at operand 2 -- `maddpt x1,sp,x3,x4'
.*: Error: expected an integer or zero register at operand 3 -- `maddpt x1,x2,sp,x4'
.*: Error: expected an integer or zero register at operand 4 -- `maddpt x1,x2,x3,sp'

.*: Error: operand mismatch -- `msubpt w1,x2,x3,x4'
.*: Info:\s+did you mean this\?
.*: Info:\s+msubpt x1, x2, x3, x4
.*: Error: operand mismatch -- `msubpt x1,w2,x3,x4'
.*: Info:\s+did you mean this\?
.*: Info:\s+msubpt x1, x2, x3, x4
.*: Error: operand mismatch -- `msubpt x1,x2,w3,x4'
.*: Info:\s+did you mean this\?
.*: Info:\s+msubpt x1, x2, x3, x4
.*: Error: operand mismatch -- `msubpt x1,x2,x3,w4'
.*: Info:\s+did you mean this\?
.*: Info:\s+msubpt x1, x2, x3, x4
.*: Error: expected an integer or zero register at operand 1 -- `msubpt sp,x2,x3,x4'
.*: Error: expected an integer or zero register at operand 2 -- `msubpt x1,sp,x3,x4'
.*: Error: expected an integer or zero register at operand 3 -- `msubpt x1,x2,sp,x4'
.*: Error: expected an integer or zero register at operand 4 -- `msubpt x1,x2,x3,sp'
