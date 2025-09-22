// Program b210613 reproduces the code reported in:
//
//  https://bugzilla.kernel.org/show_bug.cgi?id=210613
//
// This file is evolved directly from the reproducer attached to that
// bug report originally authored by Lorenz Bauer.
package main

import (
	"fmt"
	"log"

	"kernel.org/pub/linux/libs/security/libcap/cap"
)

func main() {
	if err := cap.ModeNoPriv.Set(); err != nil {
		log.Fatalf("error dropping privilege: %v", err)
	}
	fmt.Println("b210613: PASSED")
}
