// To transition from a Go call to a C function call, we are skating
// on really thin ice... Ceveat Emptor!
//
// Ref:
//   https://stackoverflow.com/questions/261419/what-registers-to-save-in-the-arm-c-calling-convention
//
// This is not strictly needed, but it makes gdb debugging less
// confusing because spacer ends up being an alias for the TEXT
// section start.
TEXT ·spacer(SB),$0
	RET

#define FINDEX(n) (8*n)

// Header to this function wrapper is the last time we can voluntarily
// yield to some other goroutine.
//
// Conventions: PC == R15, SP == R13, LR == R14, IP (scratch) = R12
TEXT ·syso(SB),$0-8
	MOVW	cFn+0(FP), R14
	MOVW    state+4(FP), R0
	BL	(R14)
	RET
