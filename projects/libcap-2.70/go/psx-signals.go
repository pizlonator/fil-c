// Program psx-signals validates that the psx mechanism can coexist
// with Go use of signals. This is an unprivilaged program derived
// from the sample code provided in this bug report:
//
//   https://bugzilla.kernel.org/show_bug.cgi?id=210533
package main

import (
	"fmt"
	"log"
	"os"
	"os/signal"
	"syscall"
	"time"

	"kernel.org/pub/linux/libs/security/libcap/psx"
)

const maxSig = 10
const prSetKeepCaps = 8

func main() {
	sig := make(chan os.Signal, maxSig)
	signal.Notify(sig, os.Interrupt)

	fmt.Print("Toggling KEEP_CAPS ")
	for i := 0; i < maxSig; i++ {
		fmt.Print(".")
		_, _, err := psx.Syscall3(syscall.SYS_PRCTL, prSetKeepCaps, uintptr(i&1), 0)
		if err != 0 {
			log.Fatalf("[%d] attempt to set KEEPCAPS (to %d) failed: %v", i, i%2, err)
		}
	}

	fmt.Println(" done")
	fmt.Print("Wait 1 second to see if unwanted signals arrive...")
	// Confirm no signals are delivered.
	select {
	case <-time.After(1 * time.Second):
		break
	case info := <-sig:
		log.Fatalf("signal received: %v", info)
	}
	fmt.Println(" none arrived")
	fmt.Println("PASSED")
}
