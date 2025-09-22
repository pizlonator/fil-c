// Program try-launching validates the cap.Launch feature.
package main

import (
	"fmt"
	"log"
	"strings"
	"syscall"

	"kernel.org/pub/linux/libs/security/libcap/cap"
)

// tryLaunching attempts to launch a bunch of programs in parallel. It
// first tries some unprivileged launches, and then (if privileged)
// tries some more ambitious ones.
func tryLaunching() {
	cwd, err := syscall.Getwd()
	if err != nil {
		log.Fatalf("no working directory: %v", err)
	}
	root := cwd[:strings.LastIndex(cwd, "/")]

	hasSysAdmin, _ := cap.GetBound(cap.SYS_ADMIN)

	vs := []struct {
		args       []string
		fail       bool
		callbackFn func(*syscall.ProcAttr, interface{}) error
		chroot     string
		iab        string
		uid        int
		gid        int
		mode       cap.Mode
		groups     []int
	}{
		{args: []string{root + "/go/ok"}},
		{
			args:   []string{root + "/progs/tcapsh-static", "--dropped=cap_chown", "--is-uid=123", "--is-gid=456", "--has-a=cap_setuid"},
			iab:    "!cap_chown,^cap_setuid,cap_sys_admin",
			uid:    123,
			gid:    456,
			groups: []int{1, 2, 3},
			fail:   syscall.Getuid() != 0 || !hasSysAdmin,
		},
		{
			args:   []string{"/ok"},
			chroot: root + "/go",
			fail:   syscall.Getuid() != 0,
		},
		{
			args: []string{root + "/progs/tcapsh-static", "--inmode=NOPRIV", "--has-no-new-privs"},
			mode: cap.ModeNoPriv,
			fail: syscall.Getuid() != 0,
		},
	}

	ps := make([]int, len(vs))
	ws := make([]syscall.WaitStatus, len(vs))

	for i, v := range vs {
		e := cap.NewLauncher(v.args[0], v.args, nil)
		e.Callback(v.callbackFn)
		if v.chroot != "" {
			e.SetChroot(v.chroot)
		}
		if v.uid != 0 {
			e.SetUID(v.uid)
		}
		if v.gid != 0 {
			e.SetGroups(v.gid, v.groups)
		}
		if v.mode != 0 {
			e.SetMode(v.mode)
		}
		if v.iab != "" {
			if iab, err := cap.IABFromText(v.iab); err != nil {
				log.Fatalf("failed to parse iab=%q: %v", v.iab, err)
			} else {
				e.SetIAB(iab)
			}
		}
		log.Printf("[%d] trying: %q\n", i, v.args)
		if ps[i], err = e.Launch(nil); err != nil {
			if v.fail {
				continue
			}
			log.Fatalf("[%d] launch %q failed: %v", i, v.args, err)
		}
	}

	for i, p := range ps {
		if p == -1 {
			continue
		}
		if pr, err := syscall.Wait4(p, &ws[i], 0, nil); err != nil {
			log.Fatalf("wait4 <%d> failed: %v", p, err)
		} else if p != pr {
			log.Fatalf("wait4 <%d> returned <%d> instead", p, pr)
		} else if ws[i] != 0 {
			if vs[i].fail {
				continue
			}
			log.Fatalf("wait4 <%d> status was %d", p, ws[i])
		}
	}
}

func main() {
	if cap.LaunchSupported {
		// The Go runtime had some OS threading bugs that
		// prevented Launch from working. Specifically, the
		// launch OS thread would get reused.
		tryLaunching()
	}
	fmt.Println("PASSED")
}
