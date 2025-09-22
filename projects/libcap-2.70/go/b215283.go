// Program b215283 requires privilege to execute and is a minimally adapted
// version of a test case provided by Lorenz Bauer as a reproducer for a
// problem he found and reported in:
//
//    https://bugzilla.kernel.org/show_bug.cgi?id=215283
package main

import (
	"fmt"
	"os"

	"kernel.org/pub/linux/libs/security/libcap/cap"
)

func main() {
	const secbits = cap.SecbitNoRoot | cap.SecbitNoSetUIDFixup

	if v, err := cap.GetProc().GetFlag(cap.Permitted, cap.SETPCAP); err != nil {
		panic(fmt.Sprintf("failed to get flag value: %v", err))
		os.Exit(1)
	} else if !v {
		fmt.Printf("test requires cap_setpcap: found %q\n", cap.GetProc())
		os.Exit(1)
	}
	if bits := cap.GetSecbits(); bits != 0 {
		fmt.Printf("test expects secbits=0 to run; found: 0%o\n", bits)
		os.Exit(1)
	}

	fmt.Println("secbits:", cap.GetSecbits(), " caps:", cap.GetProc())

	l := cap.FuncLauncher(func(interface{}) error {
		return cap.NewSet().SetProc()
	})

	if _, err := l.Launch(nil); err != nil {
		fmt.Printf("launch failed: %v\n", err)
		os.Exit(1)
	}

	fmt.Println("secbits:", cap.GetSecbits(), " caps:", cap.GetProc())

	if err := secbits.Set(); err != nil {
		fmt.Printf("set securebits: %v", err.Error())
		os.Exit(1)
	}
}
