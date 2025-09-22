// To transition from a Go call to a C function call, we are skating
// on really thin ice... Ceveat Emptor!
//
// Ref:
//   https://gitlab.com/x86-psABIs/x86-64-ABI/-/wikis/home
//
// This is not strictly needed, but it makes gdb debugging less
// confusing because spacer ends up being an alias for the TEXT
// section start.
TEXT ·spacer(SB),$0
	RET

#define RINDEX(n) (8*n)

// Header to this function wrapper is the last time we can voluntarily
// yield to some other goroutine.
TEXT ·syso(SB),$0-16
	MOVQ cFn+RINDEX(0)(FP), SI
	MOVQ state+RINDEX(1)(FP), DI
	CALL *SI
	RET
