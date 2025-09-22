// Program captrace traces processes and notices when they attempt
// kernel actions that require Effective capabilities.
//
// The reference material for developing this tool was the the book
// "Linux Observabililty with BPF" by David Calavera and Lorenzo
// Fontana.
package main

import (
	"bufio"
	"flag"
	"fmt"
	"io"
	"log"
	"os"
	"os/exec"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"time"

	"kernel.org/pub/linux/libs/security/libcap/cap"
)

var (
	bpftrace = flag.String("bpftrace", "bpftrace", "command to launch bpftrace")
	debug    = flag.Bool("debug", false, "more output")
	pid      = flag.Int("pid", -1, "PID of target process to trace (-1 = trace all)")
)

type thread struct {
	PPID, Datum int
	Value       cap.Value
	Token       string
}

// mu protects these two maps.
var mu sync.Mutex

// tids tracks which PIDs we are following.
var tids = make(map[int]int)

// cache tracks in-flight cap_capable invocations.
var cache = make(map[int]*thread)

// event adds or resolves a capability event.
func event(add bool, tid int, th *thread) {
	mu.Lock()
	defer mu.Unlock()

	if len(tids) != 0 {
		if _, ok := tids[th.PPID]; !ok {
			if *debug {
				log.Printf("dropped %d %d %v event", th.PPID, tid, *th)
			}
			return
		}
		tids[tid] = th.PPID
		tids[th.PPID] = th.PPID
	}

	if add {
		cache[tid] = th
	} else {
		if b, ok := cache[tid]; ok {
			detail := ""
			if th.Datum < 0 {
				detail = fmt.Sprintf(" (%v)", syscall.Errno(-th.Datum))
			}
			task := ""
			if th.PPID != tid {
				task = fmt.Sprintf("+{%d}", tid)
			}
			log.Printf("%-16s %d%s opt=%d %q -> %d%s", b.Token, b.PPID, task, b.Datum, b.Value, th.Datum, detail)
		}
		delete(cache, tid)
	}
}

// tailTrace tails the bpftrace command output recognizing lines of
// interest.
func tailTrace(cmd *exec.Cmd, out io.Reader) {
	launched := false
	sc := bufio.NewScanner(out)
	for sc.Scan() {
		fields := strings.Split(sc.Text(), " ")
		if len(fields) < 4 {
			continue // ignore
		}
		if !launched {
			launched = true
			mu.Unlock()
		}
		switch fields[0] {
		case "CB":
			if len(fields) < 6 {
				continue
			}
			pid, err := strconv.Atoi(fields[1])
			if err != nil {
				continue
			}
			th := &thread{
				PPID: pid,
			}
			tid, err := strconv.Atoi(fields[2])
			if err != nil {
				continue
			}
			c, err := strconv.Atoi(fields[3])
			if err != nil {
				continue
			}
			th.Value = cap.Value(c)
			aud, err := strconv.Atoi(fields[4])
			if err != nil {
				continue
			}
			th.Datum = aud
			th.Token = strings.Join(fields[5:], " ")
			event(true, tid, th)
		case "CE":
			if len(fields) < 4 {
				continue
			}
			pid, err := strconv.Atoi(fields[1])
			if err != nil {
				continue
			}
			th := &thread{
				PPID: pid,
			}
			tid, err := strconv.Atoi(fields[2])
			if err != nil {
				continue
			}
			aud, err := strconv.Atoi(fields[3])
			if err != nil {
				continue
			}
			th.Datum = aud
			event(false, tid, th)
		default:
			if *debug {
				fmt.Println("unparsable:", fields)
			}
		}
	}
	if err := sc.Err(); err != nil {
		log.Fatalf("scanning failed: %v", err)
	}
}

// tracer invokes bpftool it returns an error if the invocation fails.
func tracer() (*exec.Cmd, error) {
	cmd := exec.Command(*bpftrace, "-e", `kprobe:cap_capable {
    printf("CB %d %d %d %d %s\n", pid, tid, arg2, arg3, comm);
}
kretprobe:cap_capable {
    printf("CE %d %d %d\n", pid, tid, retval);
}`)
	out, err := cmd.StdoutPipe()
	cmd.Stderr = os.Stderr
	if err != nil {
		return nil, fmt.Errorf("unable to create stdout for %q: %v", *bpftrace, err)
	}
	mu.Lock() // Unlocked on first ouput from tracer.
	if err := cmd.Start(); err != nil {
		return nil, fmt.Errorf("failed to start %q: %v", *bpftrace, err)
	}
	go tailTrace(cmd, out)
	return cmd, nil
}

func main() {
	flag.Usage = func() {
		fmt.Fprintf(flag.CommandLine.Output(), `Usage: %s [options] [command ...]

This tool monitors cap_capable() kernel execution to summarize when
Effective Flag capabilities are checked in a running process{thread}.
The monitoring is performed indirectly using the bpftrace tool.

Each line logged has a timestamp at which the tracing program is able to
summarize the return value of the check. A return value of " -> 0" implies
the check succeeded and confirms the process{thread} does have the
specified Effective capability.

The listed "opt=" value indicates some auditing context for why the
kernel needed to check the capability was Effective.

Options:
`, os.Args[0])
		flag.PrintDefaults()
	}
	flag.Parse()

	tr, err := tracer()
	if err != nil {
		log.Fatalf("failed to start tracer: %v", err)
	}

	mu.Lock()

	if *pid != -1 {
		tids[*pid] = *pid
	} else if len(flag.Args()) != 0 {
		args := flag.Args()
		cmd := exec.Command(args[0], args[1:]...)
		cmd.Stdin = os.Stdin
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
		if err := cmd.Start(); err != nil {
			log.Fatalf("failed to start %v: %v", flag.Args(), err)
		}
		tids[cmd.Process.Pid] = cmd.Process.Pid

		// waiting for the trace to complete is racy, so we sleep
		// to obtain the last events then kill the tracer and wait
		// for it to exit. Defers are in reverse order.
		defer tr.Wait()
		defer tr.Process.Kill()
		defer time.Sleep(1 * time.Second)

		tr = cmd
	}

	mu.Unlock()
	tr.Wait()
}
