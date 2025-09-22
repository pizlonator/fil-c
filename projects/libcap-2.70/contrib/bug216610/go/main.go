// Program fib uses the psx package once, and then prints the first
// ten Fibonacci numbers.
package main

import (
	"fmt"
	"log"
	"syscall"

	"fib/fibber"

	"kernel.org/pub/linux/libs/security/libcap/psx"
)

func main() {
	pid, _, err := psx.Syscall3(syscall.SYS_GETPID, 0, 0, 0)
	if err != 0 {
		log.Fatalf("failed to get PID via psx: %v", err)
	}
	fmt.Print("psx syscall result: PID=")
	fmt.Println(pid)
	s := fibber.NewState()
	fmt.Print("fib: ", s.A, ", ", s.B)
	for i := 0; i < 8; i++ {
		s.Next()
		fmt.Print(", ", s.B)
	}
	fmt.Println(", ...")
}
