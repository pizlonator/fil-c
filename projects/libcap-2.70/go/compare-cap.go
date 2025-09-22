// Program compare-cap is a sanity check that Go's cap package is
// inter-operable with the C libcap.
package main

import (
	"log"
	"os"
	"syscall"
	"unsafe"

	"kernel.org/pub/linux/libs/security/libcap/cap"
)

// #include <stdlib.h>
// #include <sys/capability.h>
// #cgo CFLAGS: -I../libcap/include
// #cgo LDFLAGS: -L../libcap -lcap
import "C"

// tryFileCaps attempts to use the cap package to manipulate file
// capabilities. No reference to libcap in this function.
func tryFileCaps() {
	saved := cap.GetProc()

	// Capabilities we will place on a file.
	want := cap.NewSet()
	if err := want.SetFlag(cap.Permitted, true, cap.SETFCAP, cap.DAC_OVERRIDE); err != nil {
		log.Fatalf("failed to explore desired file capability: %v", err)
	}
	if err := want.SetFlag(cap.Effective, true, cap.SETFCAP, cap.DAC_OVERRIDE); err != nil {
		log.Fatalf("failed to raise the effective bits: %v", err)
	}

	if perm, err := saved.GetFlag(cap.Permitted, cap.SETFCAP); err != nil {
		log.Fatalf("failed to read capability: %v", err)
	} else if !perm {
		log.Printf("skipping file cap tests - insufficient privilege")
		return
	}

	if err := saved.ClearFlag(cap.Effective); err != nil {
		log.Fatalf("failed to drop effective: %v", err)
	}
	if err := saved.SetProc(); err != nil {
		log.Fatalf("failed to limit capabilities: %v", err)
	}

	// Failing attempt to remove capabilities.
	var empty *cap.Set
	if err := empty.SetFile(os.Args[0]); err != syscall.EPERM {
		log.Fatalf("failed to be blocked from removing filecaps: %v", err)
	}

	// The privilege we want (in the case we are root, we need the
	// DAC_OVERRIDE too).
	working, err := saved.Dup()
	if err != nil {
		log.Fatalf("failed to duplicate (%v): %v", saved, err)
	}
	if err := working.SetFlag(cap.Effective, true, cap.DAC_OVERRIDE, cap.SETFCAP); err != nil {
		log.Fatalf("failed to raise effective: %v", err)
	}

	// Critical (privilege using) section:
	if err := working.SetProc(); err != nil {
		log.Fatalf("failed to enable first effective privilege: %v", err)
	}
	// Delete capability
	if err := empty.SetFile(os.Args[0]); err != nil && err != syscall.ENODATA {
		log.Fatalf("blocked from removing filecaps: %v", err)
	}
	if got, err := cap.GetFile(os.Args[0]); err == nil {
		log.Fatalf("read deleted file caps: %v", got)
	}
	// Create file caps (this use employs the effective bit).
	if err := want.SetFile(os.Args[0]); err != nil {
		log.Fatalf("failed to set file capability: %v", err)
	}
	if err := saved.SetProc(); err != nil {
		log.Fatalf("failed to lower effective capability: %v", err)
	}
	// End of critical section.

	if got, err := cap.GetFile(os.Args[0]); err != nil {
		log.Fatalf("failed to read caps: %v", err)
	} else if is, was := got.String(), want.String(); is != was {
		log.Fatalf("read file caps do not match desired: got=%q want=%q", is, was)
	}

	// Now, do it all again but this time on an open file.
	f, err := os.Open(os.Args[0])
	if err != nil {
		log.Fatalf("failed to open %q: %v", os.Args[0], err)
	}
	defer f.Close()

	// Failing attempt to remove capabilities.
	if err := empty.SetFd(f); err != syscall.EPERM {
		log.Fatalf("failed to be blocked from fremoving filecaps: %v", err)
	}

	// For the next section, we won't set the effective bit on the file.
	want.ClearFlag(cap.Effective)

	// Critical (privilege using) section:
	if err := working.SetProc(); err != nil {
		log.Fatalf("failed to enable effective privilege: %v", err)
	}
	if err := empty.SetFd(f); err != nil && err != syscall.ENODATA {
		log.Fatalf("blocked from fremoving filecaps: %v", err)
	}
	if got, err := cap.GetFd(f); err == nil {
		log.Fatalf("read fdeleted file caps: %v", got)
	}
	// This one does not set the effective bit.
	if err := want.SetFd(f); err != nil {
		log.Fatalf("failed to fset file capability: %v", err)
	}
	if got, err := cap.GetFd(f); err != nil {
		log.Fatalf("failed to fread caps: %v", err)
	} else if is, was := got.String(), want.String(); is != was {
		log.Fatalf("fread file caps do not match desired: got=%q want=%q", is, was)
	}
	if err := empty.SetFd(f); err != nil && err != syscall.ENODATA {
		log.Fatalf("blocked from cleanup fremoving filecaps: %v", err)
	}
	if err := saved.SetProc(); err != nil {
		log.Fatalf("failed to lower effective capability: %v", err)
	}
	// End of critical section.
}

// tryProcCaps performs a set of convenience functions and compares
// the results with those seen by libcap. At the end of this function,
// the running process has no privileges at all. So exiting the
// program is the only option.
func tryProcCaps() {
	c := cap.GetProc()
	if v, err := c.GetFlag(cap.Permitted, cap.SETPCAP); err != nil {
		log.Fatalf("failed to read permitted setpcap: %v", err)
	} else if !v {
		log.Printf("skipping proc cap tests - insufficient privilege")
		return
	}
	if err := cap.SetUID(99); err != nil {
		log.Fatalf("failed to set uid=99: %v", err)
	}
	if u := syscall.Getuid(); u != 99 {
		log.Fatal("uid=99 did not take: got=%d", u)
	}
	if err := cap.SetGroups(98, 100, 101); err != nil {
		log.Fatalf("failed to set groups=98 [100, 101]: %v", err)
	}
	if g := syscall.Getgid(); g != 98 {
		log.Fatalf("gid=98 did not take: got=%d", g)
	}
	if gs, err := syscall.Getgroups(); err != nil {
		log.Fatalf("error getting groups: %v", err)
	} else if len(gs) != 2 || gs[0] != 100 || gs[1] != 101 {
		log.Fatalf("wrong of groups: got=%v want=[100 l01]", gs)
	}

	if mode := cap.GetMode(); mode != cap.ModeHybrid {
		log.Fatalf("initial mode should be 4 (HYBRID), got: %d (%v)", mode, mode)
	}

	// To distinguish PURE1E and PURE1E_INIT we need an inheritable capability set.
	working := cap.GetProc()
	if err := working.SetFlag(cap.Inheritable, true, cap.SETPCAP); err != nil {
		log.Fatalf("unable to raise inheritable bit: %v", err)
	}
	if err := working.SetProc(); err != nil {
		log.Fatalf("failed to add inheritable bit: %v", err)
	}

	for i, mode := range []cap.Mode{cap.ModePure1E, cap.ModePure1EInit, cap.ModeNoPriv} {
		if err := mode.Set(); err != nil {
			log.Fatalf("[%d] in mode=%v and failed to set mode to %d (%v): %v", i, cap.GetMode(), mode, mode, err)
		}
		if got := cap.GetMode(); got != mode {
			log.Fatalf("[%d] unable to recognise mode %d (%v), got: %d (%v)", i, mode, mode, got, got)
		}
		cM := C.cap_get_mode()
		if mode != cap.Mode(cM) {
			log.Fatalf("[%d] C and Go disagree on mode: %d vs %d", cM, mode)
		}
	}

	// The current process is now without any access to privilege.
}

func main() {
	// Use the C libcap to obtain a non-trivial capability in text form (from init).
	cC := C.cap_get_pid(1)
	if cC == nil {
		log.Fatal("basic c caps from init function failure")
	}
	defer C.cap_free(unsafe.Pointer(cC))
	var tCLen C.ssize_t
	tC := C.cap_to_text(cC, &tCLen)
	if tC == nil {
		log.Fatal("basic c init caps -> text failure")
	}
	defer C.cap_free(unsafe.Pointer(tC))

	importT := C.GoString(tC)
	if got, want := len(importT), int(tCLen); got != want {
		log.Fatalf("C string import failed: got=%d [%q] want=%d", got, importT, want)
	}

	// Validate that it can be decoded in Go.
	cGo, err := cap.FromText(importT)
	if err != nil {
		log.Fatalf("go parsing of c text import failed: %v", err)
	}

	// Validate that it matches the one directly loaded in Go.
	c, err := cap.GetPID(1)
	if err != nil {
		log.Fatalf("...failed to read init's capabilities:", err)
	}
	tGo := c.String()
	if got, want := tGo, cGo.String(); got != want {
		log.Fatalf("go text rep does not match c: got=%q, want=%q", got, want)
	}

	// Export it in text form again from Go.
	tForC := C.CString(tGo)
	defer C.free(unsafe.Pointer(tForC))

	// Validate it can be encoded in C.
	cC2 := C.cap_from_text(tForC)
	if cC2 == nil {
		log.Fatal("go text rep not parsable by c")
	}
	defer C.cap_free(unsafe.Pointer(cC2))

	// Validate that it can be exported in binary form in C
	const enoughForAnyone = 1000
	eC := make([]byte, enoughForAnyone)
	eCLen := C.cap_copy_ext(unsafe.Pointer(&eC[0]), cC2, C.ssize_t(len(eC)))
	if eCLen < 5 {
		log.Fatalf("c export yielded bad length: %d", eCLen)
	}

	// Validate that it can be imported from binary in Go
	iGo, err := cap.Import(eC[:eCLen])
	if err != nil {
		log.Fatalf("go import of c binary failed: %v", err)
	}
	if got, want := iGo.String(), importT; got != want {
		log.Fatalf("go import of c binary miscompare: got=%q want=%q", got, want)
	}

	// Validate that it can be exported in binary in Go
	iE, err := iGo.Export()
	if err != nil {
		log.Fatalf("go failed to export binary: %v", err)
	}

	// Validate that it can be imported in binary in C
	iC := C.cap_copy_int_check(unsafe.Pointer(&iE[0]), C.ssize_t(len(iE)))
	if iC == nil {
		log.Fatal("c failed to import go binary")
	}
	defer C.cap_free(unsafe.Pointer(iC))
	fC := C.cap_to_text(iC, &tCLen)
	if fC == nil {
		log.Fatal("basic c init caps -> text failure")
	}
	defer C.cap_free(unsafe.Pointer(fC))
	if got, want := C.GoString(fC), importT; got != want {
		log.Fatalf("c import from go yielded bad caps: got=%q want=%q", got, want)
	}

	// Validate that everyone agrees what all is:
	want := "=ep"
	all, err := cap.FromText("all=ep")
	if err != nil {
		log.Fatalf("unable to parse all=ep: %v", err)
	}
	if got := all.String(); got != want {
		log.Fatalf("all decode failed in Go: got=%q, want=%q", got, want)
	}

	// Validate some random values stringify consistently between
	// libcap.cap_to_text() and (*cap.Set).String().
	mb := cap.MaxBits()
	sample := cap.NewSet()
	for c := cap.Value(0); c < 7*mb; c += 3 {
		n := int(c)
		raise, f := c%mb, cap.Flag(c/mb)%3
		sample.SetFlag(f, true, raise)
		if v, err := cap.FromText(sample.String()); err != nil {
			log.Fatalf("[%d] cap to text for %q not reversible: %v", n, sample, err)
		} else if cf, err := v.Compare(sample); err != nil {
			log.Fatalf("[%d] FromText generated bad capability from %q: %v", n, sample, err)
		} else if cf != 0 {
			log.Fatalf("[%d] text import got=%q want=%q", n, v, sample)
		}
		e, err := sample.Export()
		if err != nil {
			log.Fatalf("[%d] failed to export %q: %v", n, sample, err)
		}
		i, err := cap.Import(e)
		if err != nil {
			log.Fatalf("[%d] failed to import %q: %v", n, sample, err)
		}
		if cf, err := i.Compare(sample); err != nil {
			log.Fatalf("[%d] failed to compare %q vs original:%q", n, i, sample)
		} else if cf != 0 {
			log.Fatalf("[%d] import got=%q want=%q", n, i, sample)
		}
		// Confirm that importing this portable binary
		// representation in libcap and converting to text,
		// generates the same text as Go generates. This was
		// broken prior to v0.2.41.
		cCap := C.cap_copy_int(unsafe.Pointer(&e[0]))
		if cCap == nil {
			log.Fatalf("[%d] C import failed for %q export", n, sample)
		}
		var tCLen C.ssize_t
		tC := C.cap_to_text(cCap, &tCLen)
		if tC == nil {
			log.Fatalf("[%d] basic c init caps -> text failure", n)
		}
		C.cap_free(unsafe.Pointer(cCap))
		importT := C.GoString(tC)
		C.cap_free(unsafe.Pointer(tC))
		if got, want := len(importT), int(tCLen); got != want {
			log.Fatalf("[%d] C text generated wrong length: Go=%d, C=%d", n, got, want)
		}
		if got, want := importT, sample.String(); got != want {
			log.Fatalf("[%d] C and Go text rep disparity: C=%q Go=%q", n, got, want)
		}
	}

	iab, err := cap.IABFromText("cap_chown,!cap_setuid,^cap_setgid")
	if err != nil {
		log.Fatalf("failed to initialize iab from text: %v", err)
	}
	cIAB := C.cap_iab_init()
	defer C.cap_free(unsafe.Pointer(cIAB))
	for c := cap.MaxBits(); c > 0; {
		c--
		if en, err := iab.GetVector(cap.Inh, c); err != nil {
			log.Fatalf("failed to read iab.i[%v]", c)
		} else if en {
			if C.cap_iab_set_vector(cIAB, C.CAP_IAB_INH, C.cap_value_t(int(c)), C.CAP_SET) != 0 {
				log.Fatalf("failed to set C's AIB.I %v: %v", c)
			}
		}
		if en, err := iab.GetVector(cap.Amb, c); err != nil {
			log.Fatalf("failed to read iab.a[%v]", c)
		} else if en {
			if C.cap_iab_set_vector(cIAB, C.CAP_IAB_AMB, C.cap_value_t(int(c)), C.CAP_SET) != 0 {
				log.Fatalf("failed to set C's AIB.A %v: %v", c)
			}
		}
		if en, err := iab.GetVector(cap.Bound, c); err != nil {
			log.Fatalf("failed to read iab.b[%v]", c)
		} else if en {
			if C.cap_iab_set_vector(cIAB, C.CAP_IAB_BOUND, C.cap_value_t(int(c)), C.CAP_SET) != 0 {
				log.Fatalf("failed to set C's AIB.B %v: %v", c)
			}
		}
	}
	iabC := C.cap_iab_to_text(cIAB)
	if iabC == nil {
		log.Fatalf("failed to get text from C for %q", iab)
	}
	defer C.cap_free(unsafe.Pointer(iabC))
	if got, want := C.GoString(iabC), iab.String(); got != want {
		log.Fatalf("IAB for Go and C differ: got=%q, want=%q", got, want)
	}

	// Next, we attempt to manipulate some file capabilities on
	// the running program.  These are optional, based on whether
	// the current program is capable enough and do not involve
	// any cgo calls to libcap.
	tryFileCaps()

	// Nothing left to do but exit after this one.
	tryProcCaps()
	log.Printf("compare-cap success!")
}
