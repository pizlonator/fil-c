// Program explore is evolved from the code discussed in more depth
// here:
//
//   https://github.com/golang/go/issues/3405
//
// The code here demonstrates that while PR_SET_NO_NEW_PRIVS only
// applies to the calling thread, since
// https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=103502a35cfce0710909da874f092cb44823ca03
// the seccomp filter application forces the setting to be mirrored on
// all the threads of a process.
//
// Based on the command line options, we can manipulate the program to
// behave in various ways. Example command lines:
//
//   sudo ./explore
//   sudo ./explore --kill=false
//   sudo ./explore --kill=false --errno=0
//
// Supported Go toolchains are after go1.10. Those prior to go1.15
// require this environment variable to be set to build successfully:
//
//   export CGO_LDFLAGS_ALLOW="-Wl,-?-wrap[=,][^-.@][^,]*"
//
// Go toolchains go1.16+ can be compiled CGO_ENABLED=0 too,
// demonstrating native nocgo support for seccomp features.
package main

import (
	"flag"
	"fmt"
	"log"
	"runtime"
	"syscall"
	"time"
	"unsafe"

	"kernel.org/pub/linux/libs/security/libcap/psx"
)

var (
	withPSX = flag.Bool("psx", false, "use the psx mechanism to invoke prctl syscall")
	delays  = flag.Bool("delays", false, "use this to pause the program at various places")
	kill    = flag.Bool("kill", true, "kill the process if setuid attempted")
	errno   = flag.Int("errno", int(syscall.ENOTSUP), "if kill is false, block syscall and return this errno")
)

const (
	prSetNoNewPrivs = 38

	sysSeccomp             = 317        // x86_64 syscall number
	seccompSetModeFilter   = 1          // uses user-supplied filter.
	seccompFilterFlagTsync = (1 << 0)   // mirror filtering on all threads.
	seccompRetErrno        = 0x00050000 // returns an errno
	seccompRetData         = 0x0000ffff // mask for RET data payload (ex. errno)
	seccompRetKillProcess  = 0x80000000 // kill the whole process immediately
	seccompRetTrap         = 0x00030000 // disallow and force a SIGSYS
	seccompRetAllow        = 0x7fff0000

	bpfLd  = 0x00
	bpfJmp = 0x05
	bpfRet = 0x06

	bpfW = 0x00

	bpfAbs = 0x20
	bpfJeq = 0x10

	bpfK = 0x00

	auditArchX86_64 = 3221225534 // HACK: I don't understand this value
	archNr          = auditArchX86_64

	syscallNr = 0
)

// SockFilter is a single filter block.
type SockFilter struct {
	// Code is the filter code instruction.
	Code uint16
	// Jt is the target for a true result from the code execution.
	Jt uint8
	// Jf is the target for a false result from the code execution.
	Jf uint8
	// K is a generic multiuse field
	K uint32
}

// SockFProg is a
type SockFProg struct {
	// Len is the number of contiguous SockFilter blocks that can
	// be found at *Filter.
	Len uint16
	// Filter is the address of the first SockFilter block of a
	// program sequence.
	Filter *SockFilter
}

// SockFilterSlice is a subprogram filter.
type SockFilterSlice []SockFilter

func bpfStmt(code uint16, k uint32) SockFilter {
	return SockFilter{code, 0, 0, k}
}

func bpfJump(code uint16, k uint32, jt uint8, jf uint8) SockFilter {
	return SockFilter{code, jt, jf, k}
}

func validateArchitecture() []SockFilter {
	return []SockFilter{
		bpfStmt(bpfLd+bpfW+bpfAbs, 4), // HACK: I don't understand this 4.
		bpfJump(bpfJmp+bpfJeq+bpfK, archNr, 1, 0),
		bpfStmt(bpfRet+bpfK, seccompRetKillProcess),
	}
}

func examineSyscall() []SockFilter {
	return []SockFilter{
		bpfStmt(bpfLd+bpfW+bpfAbs, syscallNr),
	}
}

func allowSyscall(syscallNum uint32) []SockFilter {
	return []SockFilter{
		bpfJump(bpfJmp+bpfJeq+bpfK, syscallNum, 0, 1),
		bpfStmt(bpfRet+bpfK, seccompRetAllow),
	}
}

func disallowSyscall(syscallNum, errno uint32) []SockFilter {
	return []SockFilter{
		bpfJump(bpfJmp+bpfJeq+bpfK, syscallNum, 0, 1),
		bpfStmt(bpfRet+bpfK, seccompRetErrno|(errno&seccompRetData)),
	}
}

func killProcess() []SockFilter {
	return []SockFilter{
		bpfStmt(bpfRet+bpfK, seccompRetKillProcess),
	}
}

func notifyProcessAndDie() []SockFilter {
	return []SockFilter{
		bpfStmt(bpfRet+bpfK, seccompRetTrap),
	}
}

func trapOnSyscall(syscallNum uint32) []SockFilter {
	return []SockFilter{
		bpfJump(bpfJmp+bpfJeq+bpfK, syscallNum, 0, 1),
		bpfStmt(bpfRet+bpfK, seccompRetTrap),
	}
}

func allGood() []SockFilter {
	return []SockFilter{
		bpfStmt(bpfRet+bpfK, seccompRetAllow),
	}
}

// prctl executes the prctl - unless the --psx commandline argument is
// used, this is on a single thread.
//go:uintptrescapes
func prctl(option, arg1, arg2, arg3, arg4, arg5 uintptr) error {
	var e syscall.Errno
	if *withPSX {
		_, _, e = psx.Syscall6(syscall.SYS_PRCTL, option, arg1, arg2, arg3, arg4, arg5)
	} else {
		_, _, e = syscall.RawSyscall6(syscall.SYS_PRCTL, option, arg1, arg2, arg3, arg4, arg5)
	}
	if e != 0 {
		return e
	}
	if *delays {
		fmt.Println("prctl'd - check now")
		time.Sleep(1 * time.Minute)
	}
	return nil
}

// SeccompSetModeFilter is our wrapper for performing our seccomp system call.
//go:uintptrescapes
func SeccompSetModeFilter(prog *SockFProg) error {
	if _, _, e := syscall.RawSyscall(sysSeccomp, seccompSetModeFilter, seccompFilterFlagTsync, uintptr(unsafe.Pointer(prog))); e != 0 {
		return e
	}
	return nil
}

var empty func()

func lockProcessThread(pick bool) {
	// Make sure we are
	pid := uintptr(syscall.Getpid())
	runtime.LockOSThread()
	for {
		tid, _, _ := syscall.RawSyscall(syscall.SYS_GETTID, 0, 0, 0)
		if (tid == pid) == pick {
			fmt.Println("validated TID:", tid, "== PID:", pid, "is", pick)
			break
		}
		runtime.UnlockOSThread()
		go func() {
			time.Sleep(1 * time.Microsecond)
		}()
		runtime.Gosched()
		runtime.LockOSThread()
	}
}

// applyPolicy uploads the program sequence.
func applyPolicy(prog *SockFProg) {
	// Without PSX we can't guarantee the thread we execute the
	// seccomp call on will be the same one that we disabled new
	// privs on. With PSX, the disabling of new privs is mirrored
	// on all threads.
	if !*withPSX {
		lockProcessThread(false)
		defer runtime.UnlockOSThread()
	}

	// This is required to load a filter without privilege.
	if err := prctl(prSetNoNewPrivs, 1, 0, 0, 0, 0); err != nil {
		log.Fatalf("Prctl(PR_SET_NO_NEW_PRIVS): %v", err)
	}

	fmt.Println("Applying syscall policy...")
	if err := SeccompSetModeFilter(prog); err != nil {
		log.Fatalf("seccomp_set_mode_filter: %v", err)
	}
	fmt.Println("...Policy applied")
}

func main() {
	flag.Parse()

	if *delays {
		fmt.Println("check first", syscall.Getpid())
		time.Sleep(60 * time.Second)
	}

	var filter []SockFilter
	filter = append(filter, validateArchitecture()...)

	// Grab the system call number.
	filter = append(filter, examineSyscall()...)

	// List disallowed syscalls.
	for _, x := range []uint32{
		syscall.SYS_SETUID,
	} {
		if *kill {
			filter = append(filter, trapOnSyscall(x)...)
		} else {
			filter = append(filter, disallowSyscall(x, uint32(*errno))...)
		}
	}

	filter = append(filter, allGood()...)

	prog := &SockFProg{
		Len:    uint16(len(filter)),
		Filter: &filter[0],
	}

	applyPolicy(prog)

	// Ensure we are running on the TID=PID.
	lockProcessThread(true)

	log.Print("Now it is time to try to run something privileged...")
	if _, _, e := syscall.RawSyscall(syscall.SYS_SETUID, 1, 0, 0); e != 0 {
		log.Fatalf("setuid failed with an error: %v", e)
	}
	log.Print("Looked like that worked, but it really didn't: uid == ", syscall.Getuid(), " != 1")
}
