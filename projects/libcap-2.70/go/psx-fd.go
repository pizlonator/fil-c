package main

import (
	"log"
	"os"
	"syscall"
	"time"

	"kernel.org/pub/linux/libs/security/libcap/psx"
)

const prSetKeepCaps = 8

func main() {
	r, w, err := os.Pipe()
	if err != nil {
		log.Fatalf("failed to obtain pipe: %v", err)
	}
	data := make([]byte, 2+r.Fd())
	go r.Read(data)
	time.Sleep(500 * time.Millisecond)
	psx.Syscall3(syscall.SYS_PRCTL, prSetKeepCaps, 1, 0)
	w.Close()
	r.Close()
}
