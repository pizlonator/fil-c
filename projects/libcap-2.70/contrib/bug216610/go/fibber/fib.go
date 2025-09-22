// Package fibber implements a Fibonacci sequence generator using a C
// coded compute kernel (a .syso file).
package fibber

import (
	"unsafe"
)

// State is the native Go form of the C.state structure.
type State struct {
	B, A uint32
}

// cPtr converts State into a C pointer suitable as an argument for
// sysoCaller.
func (s *State) cPtr() unsafe.Pointer {
	return unsafe.Pointer(&s.B)
}

// NewState initializes a Fibonacci Number sequence generator.  Upon
// return s.A=0 and s.B=1 are the first two numbers in the sequence.
func NewState() *State {
	s := &State{}
	syso__fib_init.call(s.cPtr())
	return s
}

// Next advances the state to the next number in the sequence. Upon
// return, s.B is the most recently calculated value.
func (s *State) Next() {
	syso__fib_next.call(s.cPtr())
}
