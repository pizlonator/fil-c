// Program setid demonstrates how the to use the cap and/or psx packages to
// change the uid, gids of a program.
//
// A long writeup explaining how to use it in various different ways
// is available:
//
//   https://sites.google.com/site/fullycapable/Home/using-go-to-set-uid-and-gids
package main

import (
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"strconv"
	"strings"
	"syscall"
	"unsafe"

	"kernel.org/pub/linux/libs/security/libcap/cap"
	"kernel.org/pub/linux/libs/security/libcap/psx"
)

var (
	uid      = flag.Int("uid", -1, "specify a uid with a value other than (euid)")
	gid      = flag.Int("gid", -1, "specify a gid with a value other than (egid)")
	drop     = flag.Bool("drop", true, "drop privilege once IDs have been changed")
	suppl    = flag.String("suppl", "", "comma separated list of groups")
	withCaps = flag.Bool("caps", true, "raise capabilities to setuid/setgid")
)

// setIDWithCaps uses the cap.SetUID and cap.SetGroups functions.
func setIDsWithCaps(setUID, setGID int, gids []int) {
	if err := cap.SetGroups(setGID, gids...); err != nil {
		log.Fatalf("group setting failed: %v", err)
	}
	if err := cap.SetUID(setUID); err != nil {
		log.Fatalf("user setting failed: %v", err)
	}
}

func main() {
	flag.Parse()

	showIDs("before", false, syscall.Getuid(), syscall.Getgid())

	gids := splitToInts()
	setGID := *gid
	if *gid == -1 {
		setGID = syscall.Getegid()
	}
	setUID := *uid
	if *uid == -1 {
		setUID = syscall.Getuid()
	}

	if *withCaps {
		setIDsWithCaps(setUID, setGID, gids)
	} else {
		if _, _, err := psx.Syscall3(syscall.SYS_SETGID, uintptr(setGID), 0, 0); err != 0 {
			log.Fatalf("failed to setgid(%d): %v", setGID, err)
		}
		if len(gids) != 0 {
			gids32 := []int32{int32(setGID)}
			for _, g := range gids {
				gids32 = append(gids32, int32(g))
			}
			if _, _, err := psx.Syscall3(syscall.SYS_SETGROUPS, uintptr(unsafe.Pointer(&gids32[0])), 0, 0); err != 0 {
				log.Fatalf("failed to setgroups(%d, %v): %v", setGID, gids32, err)
			}
		}
		if _, _, err := psx.Syscall3(syscall.SYS_SETUID, uintptr(setUID), 0, 0); err != 0 {
			log.Fatalf("failed to setgid(%d): %v", setUID, err)
		}
	}

	if *drop {
		if err := cap.NewSet().SetProc(); err != nil {
			log.Fatalf("unable to drop privilege: %v", err)
		}
	}

	showIDs("after", true, setUID, setGID)
}

// splitToInts parses a comma separated string to a slice of integers.
func splitToInts() (ret []int) {
	if *suppl == "" {
		return
	}
	a := strings.Split(*suppl, ",")
	for _, s := range a {
		n, err := strconv.Atoi(s)
		if err != nil {
			log.Fatalf("bad supplementary group [%q]: %v", s, err)
		}
		ret = append(ret, n)
	}
	return
}

// dumpStatus explores the current process /proc/task/* status files
// for matching values.
func dumpStatus(testCase string, validate bool, filter, expect string) bool {
	fmt.Printf("%s:\n", testCase)
	var failed bool
	pid := syscall.Getpid()
	fs, err := ioutil.ReadDir(fmt.Sprintf("/proc/%d/task", pid))
	if err != nil {
		log.Fatal(err)
	}
	for _, f := range fs {
		tf := fmt.Sprintf("/proc/%s/status", f.Name())
		d, err := ioutil.ReadFile(tf)
		if err != nil {
			fmt.Println(tf, err)
			failed = true
			continue
		}
		lines := strings.Split(string(d), "\n")
		for _, line := range lines {
			if strings.HasPrefix(line, filter) {
				fails := line != expect
				failure := ""
				if fails && validate {
					failed = fails
					failure = " (bad)"
				}
				fmt.Printf("%s %s%s\n", tf, line, failure)
				break
			}
		}
	}
	return failed
}

// showIDs dumps the thread map out of the /proc/<proc>/tasks
// filesystem to confirm that all of the threads associated with the
// process have the same uid/gid values. Note, the code does not
// attempt to validate the supplementary groups at present.
func showIDs(test string, validate bool, wantUID, wantGID int) {
	fmt.Printf("%s capability state: %q\n", test, cap.GetProc())

	failed := dumpStatus(test+" gid", validate, "Gid:", fmt.Sprintf("Gid:\t%d\t%d\t%d\t%d", wantGID, wantGID, wantGID, wantGID))

	failed = dumpStatus(test+" uid", validate, "Uid:", fmt.Sprintf("Uid:\t%d\t%d\t%d\t%d", wantUID, wantUID, wantUID, wantUID)) || failed

	if validate && failed {
		log.Fatal("did not observe desired *id state")
	}
}
