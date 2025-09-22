// Program mismatch should panic because the syscall being requested
// never returns consistent results.
package main

import (
	"fmt"
	"syscall"

	"kernel.org/pub/linux/libs/security/libcap/psx"
)

func main() {
	tid, _, err := psx.Syscall3(syscall.SYS_GETTID, 0, 0, 0)
	fmt.Printf("gettid() -> %d: %v\n", tid, err)
}
