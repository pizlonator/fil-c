// Program ok exits with status zero. We use it as a chroot test.
// To avoid any confusion, it needs to be linked statically.
package main

import "os"

func main() {
	os.Exit(0)
}
