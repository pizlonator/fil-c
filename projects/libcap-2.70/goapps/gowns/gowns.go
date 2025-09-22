// Program gowns is a small program to explore and demonstrate using
// Go to Wrap a child in a NameSpace under Linux.
//
// Note, this program is under active development and should not be
// considered stable. That is, it is more a worked example and may
// change command line arguments and behavior from release to release.
// Should it become stable, I'll remove this comment.
package main

import (
	"errors"
	"flag"
	"fmt"
	"log"
	"os"
	"strings"
	"syscall"

	"kernel.org/pub/linux/libs/security/libcap/cap"
)

// nsDetail is how we summarize the type of namespace we want to
// enter.
type nsDetail struct {
	// uid holds the uid for the base user in this namespace (defaults to getuid).
	uid int

	// uidMap holds the namespace mapping of uid values.
	uidMap []syscall.SysProcIDMap

	// gid holds the gid for the base user in this namespace (defaults to getgid).
	gid int

	// uidMap holds the namespace mapping of gid values.
	gidMap []syscall.SysProcIDMap
}

var (
	baseID = flag.Int("base", -1, "base id for uids and gids (-1 = invoker's uid)")
	uid    = flag.Int("uid", -1, "uid of the hosting user")
	gid    = flag.Int("gid", -1, "gid of the hosting user")
	iab    = flag.String("iab", "", "IAB string for inheritable capabilities")
	mode   = flag.String("mode", "", "force a libcap mode (capsh --modes for list)")

	ns   = flag.Bool("ns", false, "enable user namespace features")
	uids = flag.String("uids", "", "comma separated UID ranges to map contiguously (req. CAP_SETUID)")
	gids = flag.String("gids", "", "comma separated GID ranges to map contiguously (req. CAP_SETGID)")

	shell = flag.String("shell", "/bin/bash", "shell to be launched")
	debug = flag.Bool("verbose", false, "more verbose output")
)

// r holds a base and count for a contiguous range.
type r struct {
	base, count int
}

// ranges unpacks numerical ranges.
func ranges(s string) []r {
	if s == "" {
		return nil
	}
	var rs []r
	for _, n := range strings.Split(s, ",") {
		var base, upper int
		if _, err := fmt.Sscanf(n, "%d-%d", &base, &upper); err == nil {
			if upper < base {
				log.Fatalf("invalid range: [%d-%d]", base, upper)
			}
			rs = append(rs, r{
				base:  base,
				count: 1 + upper - base,
			})
		} else if _, err := fmt.Sscanf(n, "%d", &base); err == nil {
			rs = append(rs, r{
				base:  base,
				count: 1,
			})
		} else {
			log.Fatalf("unable to parse range [%s]", n)
		}
	}
	return rs
}

// restart launches the program again with the remaining arguments.
func restart() {
	log.Fatalf("failed to restart: flags: %q %q", os.Args[0], flag.Args()[1:])
}

// errUnableToSetup is how nsSetup fails.
var errUnableToSetup = errors.New("data was not in supported format")

// nsSetup is the callback used to enter the namespace for the user
// via callback in the cap.Launcher mechanism.
func nsSetup(pa *syscall.ProcAttr, data interface{}) error {
	nsD, ok := data.(nsDetail)
	if !ok {
		return errUnableToSetup
	}

	if pa.Sys == nil {
		pa.Sys = &syscall.SysProcAttr{}
	}
	pa.Sys.Cloneflags |= syscall.CLONE_NEWUSER
	pa.Sys.UidMappings = nsD.uidMap
	pa.Sys.GidMappings = nsD.gidMap
	return nil
}

func parseRanges(detail *nsDetail, ids string, id int) []syscall.SysProcIDMap {
	base := *baseID
	if base < 0 {
		base = detail.uid
	}

	list := []syscall.SysProcIDMap{
		syscall.SysProcIDMap{
			ContainerID: base,
			HostID:      id,
			Size:        1,
		},
	}

	base++
	for _, next := range ranges(ids) {
		list = append(list,
			syscall.SysProcIDMap{
				ContainerID: base,
				HostID:      next.base,
				Size:        next.count,
			})
		base += next.count
	}
	return list
}

func main() {
	flag.Parse()

	detail := nsDetail{
		gid: syscall.Getgid(),
	}

	thisUID := syscall.Getuid()
	switch *uid {
	case -1:
		detail.uid = thisUID
	default:
		detail.uid = *uid
	}
	detail.uidMap = parseRanges(&detail, *uids, detail.uid)

	thisGID := syscall.Getgid()
	switch *gid {
	case -1:
		detail.gid = thisGID
	default:
		detail.gid = *gid
	}
	detail.gidMap = parseRanges(&detail, *gids, detail.gid)

	unparsed := flag.Args()

	arg0 := *shell
	skip := 0
	var w *cap.Launcher
	if len(unparsed) > 0 {
		switch unparsed[0] {
		case "==":
			arg0 = os.Args[0]
			skip++
		}
	}

	w = cap.NewLauncher(arg0, append([]string{arg0}, unparsed[skip:]...), nil)
	if *ns {
		// Include the namespace setup callback with the launcher.
		w.Callback(nsSetup)
	}

	if thisUID != detail.uid {
		w.SetUID(detail.uid)
	}

	if thisGID != detail.gid {
		w.SetGroups(detail.gid, nil)
	}

	if *iab != "" {
		ins, err := cap.IABFromText(*iab)
		if err != nil {
			log.Fatalf("--iab=%q parsing issue: %v", err)
		}
		w.SetIAB(ins)
	}

	if *mode != "" {
		for m := cap.Mode(1); ; m++ {
			if s := m.String(); s == "UNKNOWN" {
				log.Fatalf("mode %q is unknown", *mode)
			} else if s == *mode {
				w.SetMode(m)
				break
			}
		}
	}

	// The launcher can enable more functionality if involked with
	// effective capabilities.
	have := cap.GetProc()
	for _, c := range []cap.Value{cap.SETUID, cap.SETGID} {
		if canDo, err := have.GetFlag(cap.Permitted, c); err != nil {
			log.Fatalf("failed to explore process capabilities, %q for %q", have, c)
		} else if canDo {
			if err := have.SetFlag(cap.Effective, true, c); err != nil {
				log.Fatalf("failed to raise effective capability: \"%v e+%v\"", have, c)
			}
		}
	}
	if err := have.SetProc(); err != nil {
		log.Fatalf("privilege assertion %q failed: %v", have, err)
	}

	if *debug {
		if *ns {
			fmt.Println("launching namespace")
		} else {
			fmt.Println("launching without namespace")
		}
	}

	pid, err := w.Launch(detail)
	if err != nil {
		log.Fatalf("launch failed: %v", err)
	}
	if err := cap.NewSet().SetProc(); err != nil {
		log.Fatalf("gowns could not drop privilege: %v", err)
	}

	p, err := os.FindProcess(pid)
	if err != nil {
		log.Fatalf("cannot find process: %v", err)
	}
	state, err := p.Wait()
	if err != nil {
		log.Fatalf("waiting failed: %v", err)
	}

	if *debug {
		fmt.Println("process exited:", state)
	}
}
