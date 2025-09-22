// Program captree explores a process tree rooted in the supplied
// argument(s) and displays a process tree indicating the capabilities
// of all the dependent PID values.
//
// This was inspired by the pstree utility. The key idea here, however,
// is to explore a process tree for capability state.
//
// Each line of output is intended to capture a brief representation
// of the capability state of a process (both *Set and *IAB) and
// for its related threads.
//
// Ex:
//
//   $ bash -c 'exec captree $$'
//   --captree(9758+{9759,9760,9761,9762})
//
// In the normal case, such as the above, where the targeted process
// is not privileged, no distracting capability strings are displayed.
// Where a process is thread group leader to a set of other thread
// ids, they are listed as `+{...}`.
//
// For privileged binaries, we have:
//
//   $ captree 551
//   --polkitd(551) "=ep"
//     :>-gmain{552} "=ep"
//     :>-gdbus{555} "=ep"
//
// That is, the text representation of the process capability state is
// displayed in double quotes "..." as a suffix to the process/thread.
// If the name of any thread of this process, or its own capability
// state, is in some way different from the primary process then it is
// displayed on a subsequent line prefixed with ":>-" and threads
// sharing name and capability state are listed on that line. Here we
// have two sub-threads with the same capability state, but unique
// names.
//
// Sometimes members of a process group have different capabilities:
//
//   $ captree 1368
//   --dnsmasq(1368) "cap_net_bind_service,cap_net_admin,cap_net_raw=ep"
//     +-dnsmasq(1369) "=ep"
//
// Where the A and B components of the IAB tuple are non-default, the
// output also includes these:
//
//   $ captree 925
//   --dbus-broker-lau(925) [!cap_sys_rawio,!cap_mknod]
//     +-dbus-broker(965) "cap_audit_write=eip" [!cap_sys_rawio,!cap_mknod,cap_audit_write]
//
// That is, the `[...]` appendage captures the IAB text representation
// of that tuple. Note, if only the I part of that tuple is
// non-default, it is already captured in the quoted process
// capability state, so the IAB tuple is omitted.
//
// To view the complete system process map, rooted at the kernel, try
// this:
//
//   $ captree 0
//
// To view a specific binary (as named in /proc/<PID>/status as 'Name:
// ...'), matched by a glob, try this:
//
//   $ captree 'cap*ree'
//
// The quotes might be needed to avoid the '*' confusing your shell.
package main

import (
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"sync"

	"kernel.org/pub/linux/libs/security/libcap/cap"
)

var (
	proc    = flag.String("proc", "/proc", "root of proc filesystem")
	depth   = flag.Int("depth", 0, "how many processes deep (0=all)")
	verbose = flag.Bool("verbose", false, "display empty capabilities")
	color   = flag.Bool("color", true, "color targeted PIDs on tty in red")
	colour  = flag.Bool("colour", true, "colour targeted PIDs on tty in red")
)

type task struct {
	mu       sync.Mutex
	viewed   bool
	depth    int
	pid      string
	cmd      string
	cap      *cap.Set
	iab      *cap.IAB
	parent   string
	threads  []*task
	children []string
}

func (ts *task) String() string {
	return fmt.Sprintf("%s %q [%v] %s %v %v", ts.cmd, ts.cap, ts.iab, ts.parent, ts.threads, ts.children)
}

var (
	wg      sync.WaitGroup
	mu      sync.Mutex
	colored bool
)

func isATTY() bool {
	s, err := os.Stdout.Stat()
	if err == nil && (s.Mode()&os.ModeCharDevice) != 0 {
		return true
	}
	return false
}

func highlight(text string) string {
	if colored {
		return fmt.Sprint("\033[31m", text, "\033[0m")
	}
	return text
}

func (ts *task) fill(pid string, n int, thread bool) {
	defer wg.Done()
	wg.Add(1)
	go func() {
		defer wg.Done()
		c, _ := cap.GetPID(n)
		iab, _ := cap.IABGetPID(n)
		ts.mu.Lock()
		defer ts.mu.Unlock()
		ts.pid = pid
		ts.cap = c
		ts.iab = iab
	}()

	d, err := ioutil.ReadFile(fmt.Sprintf("%s/%s/status", *proc, pid))
	if err != nil {
		ts.mu.Lock()
		defer ts.mu.Unlock()
		ts.cmd = "<zombie>"
		ts.parent = "1"
		return
	}
	for _, line := range strings.Split(string(d), "\n") {
		if strings.HasPrefix(line, "Name:\t") {
			ts.mu.Lock()
			ts.cmd = line[6:]
			ts.mu.Unlock()
			continue
		}
		if strings.HasPrefix(line, "PPid:\t") {
			ppid := line[6:]
			if ppid == pid {
				continue
			}
			ts.mu.Lock()
			ts.parent = ppid
			ts.mu.Unlock()
		}
	}
	if thread {
		return
	}

	threads, err := ioutil.ReadDir(fmt.Sprintf("%s/%s/task", *proc, pid))
	if err != nil {
		return
	}
	var ths []*task
	for _, t := range threads {
		tid := t.Name()
		if tid == pid {
			continue
		}
		n, err := strconv.ParseInt(pid, 10, 64)
		if err != nil {
			continue
		}
		thread := &task{}
		wg.Add(1)
		go thread.fill(tid, int(n), true)
		ths = append(ths, thread)
	}
	ts.mu.Lock()
	defer ts.mu.Unlock()
	ts.threads = ths
}

var empty = cap.NewSet()
var noiab = cap.IABInit()

// rDump prints out the tree of processes rooted at pid.
func rDump(pids map[string]*task, requested map[string]bool, pid, stub, lstub, estub string, depth int) {
	info, ok := pids[pid]
	if !ok {
		panic("programming error")
		return
	}
	if info.viewed {
		// This process (tree) has already been viewed so skip
		// repeating it.
		return
	}
	info.viewed = true

	c := ""
	set := info.cap
	if set != nil {
		if val, _ := set.Cf(empty); val != 0 || *verbose {
			c = fmt.Sprintf(" %q", set)
		}
	}
	iab := ""
	tup := info.iab
	if tup != nil {
		if val, _ := tup.Cf(noiab); val.Has(cap.Bound) || val.Has(cap.Amb) || *verbose {
			iab = fmt.Sprintf(" [%s]", tup)
		}
	}
	var misc []*task
	var same []string
	for _, t := range info.threads {
		if val, _ := t.cap.Cf(set); val != 0 {
			misc = append(misc, t)
			continue
		}
		if val, _ := t.iab.Cf(tup); val != 0 {
			misc = append(misc, t)
			continue
		}
		if t.cmd != info.cmd {
			misc = append(misc, t)
			continue
		}
		same = append(same, t.pid)
	}
	tids := ""
	if len(same) != 0 {
		tids = fmt.Sprintf("+{%s}", strings.Join(same, ","))
	}
	hPID := pid
	if requested[pid] {
		hPID = highlight(pid)
		requested[pid] = false
	}
	fmt.Printf("%s%s%s(%s%s)%s%s\n", stub, lstub, info.cmd, hPID, tids, c, iab)
	// loop over any threads that differ in capability state.
	for len(misc) != 0 {
		this := misc[0]
		var nmisc []*task
		var hPID = this.pid
		if requested[this.pid] {
			hPID = highlight(this.pid)
			requested[this.pid] = false
		}
		same := []string{hPID}
		for _, t := range misc[1:] {
			if val, _ := this.cap.Cf(t.cap); val != 0 {
				nmisc = append(nmisc, t)
				continue
			}
			if val, _ := this.iab.Cf(t.iab); val != 0 {
				nmisc = append(nmisc, t)
				continue
			}
			if this.cmd != t.cmd {
				nmisc = append(nmisc, t)
				continue
			}
			hPID = t.pid
			if requested[t.pid] {
				hPID = highlight(t.pid)
				requested[t.pid] = false
			}
			same = append(same, hPID)
		}
		c := ""
		set := this.cap
		if set != nil {
			if val, _ := set.Cf(empty); val != 0 || *verbose {
				c = fmt.Sprintf(" %q", set)
			}
		}
		iab := ""
		tup := this.iab
		if tup != nil {
			if val, _ := tup.Cf(noiab); val.Has(cap.Bound) || val.Has(cap.Amb) || *verbose {
				iab = fmt.Sprintf(" [%s]", tup)
			}
		}
		fmt.Printf("%s%s:>-%s{%s}%s%s\n", stub, estub, this.cmd, strings.Join(same, ","), c, iab)
		misc = nmisc
	}
	if depth == 1 {
		return
	}
	if depth > 1 {
		depth--
	}
	x := info.children
	sort.Slice(x, func(i, j int) bool {
		a, _ := strconv.Atoi(x[i])
		b, _ := strconv.Atoi(x[j])
		return a < b
	})
	stub = fmt.Sprintf("%s%s", stub, estub)
	lstub = "+-"
	for i, cid := range x {
		estub := "| "
		if i+1 == len(x) {
			estub = "  "
		}
		rDump(pids, requested, cid, stub, lstub, estub, depth)
	}
}

func findPIDs(list []string, pids map[string]*task, glob string) <-chan string {
	finds := make(chan string)
	go func() {
		defer close(finds)
		found := false
		// search for PIDs, if found exit.
		for _, pid := range list {
			match, _ := filepath.Match(glob, pids[pid].cmd)
			if !match {
				continue
			}
			found = true
			finds <- pid
		}
		if found {
			return
		}
		fmt.Printf("no process matched %q\n", glob)
		os.Exit(1)
	}()
	return finds
}

func setDepth(pids map[string]*task, pid string) int {
	if pid == "0" {
		return 0
	}
	x := pids[pid]
	if x.depth == 0 {
		x.depth = setDepth(pids, x.parent) + 1
	}
	return x.depth
}

func main() {
	flag.Usage = func() {
		fmt.Fprintf(flag.CommandLine.Output(), "Usage: %s [options] [pid|glob] ...\nOptions:\n", os.Args[0])
		flag.PrintDefaults()
	}
	flag.Parse()

	// Honor the command line request if possible.
	colored = *color && *colour && isATTY()

	// Just in case the user wants to override this, we set the
	// cap package up to find it.
	cap.ProcRoot(*proc)

	pids := make(map[string]*task)
	pids["0"] = &task{
		cmd: "<kernel>",
	}

	// Ingest the entire process tree
	fs, err := ioutil.ReadDir(*proc)
	if err != nil {
		log.Fatalf("unable to open %q: %v", *proc, err)
	}
	for _, f := range fs {
		pid := f.Name()
		n, err := strconv.ParseInt(pid, 10, 64)
		if err != nil {
			continue
		}
		ts := &task{}
		mu.Lock()
		pids[pid] = ts
		mu.Unlock()
		wg.Add(1)
		go ts.fill(pid, int(n), false)
	}
	wg.Wait()

	var list []string
	for pid, ts := range pids {
		setDepth(pids, pid)
		list = append(list, pid)
		if pid == "0" {
			continue
		}
		if pts, ok := pids[ts.parent]; ok {
			pts.children = append(pts.children, pid)
		}
	}

	// Sort the process tree by tree depth - shallowest first,
	// with numerical order breaking ties.
	sort.Slice(list, func(i, j int) bool {
		x, y := pids[list[i]], pids[list[j]]
		if x.depth == y.depth {
			a, _ := strconv.Atoi(x.pid)
			b, _ := strconv.Atoi(y.pid)
			return a < b
		}
		return x.depth < y.depth
	})

	args := flag.Args()
	if len(args) == 0 {
		args = []string{"1"}
	}

	wanted := make(map[string]int)
	requested := make(map[string]bool)
	for _, pid := range args {
		if _, err := strconv.ParseUint(pid, 10, 64); err == nil {
			requested[pid] = true
			if info, ok := pids[pid]; ok {
				wanted[pid] = info.depth
				continue
			}
			if requested[pid] {
				continue
			}
			requested[pid] = true
			continue
		}
		for pid := range findPIDs(list, pids, pid) {
			requested[pid] = true
			if info, ok := pids[pid]; ok {
				wanted[pid] = info.depth
			}
		}
	}

	var noted []string
	for pid := range wanted {
		noted = append(noted, pid)
	}
	sort.Slice(noted, func(i, j int) bool {
		return wanted[noted[i]] < wanted[noted[j]]
	})

	// We've boiled down the processes to a unique set of targets.
	for _, pid := range noted {
		rDump(pids, requested, pid, "", "--", "  ", *depth)
	}

	for pid, missed := range requested {
		if missed {
			fmt.Println("[PID", pid, "not found]")
		}
	}
}
