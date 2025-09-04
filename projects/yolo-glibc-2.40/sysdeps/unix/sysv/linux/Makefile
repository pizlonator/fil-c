# Option to pass to Python scripts to set the C compiler.  Rewriting
# MODULE_NAME is required to enable the _ISOMAC verbatim header
# environment.
sysdeps-linux-python-cc = \
  --cc="$(CC) $(patsubst -DMODULE_NAME=%,-DMODULE_NAME=testsuite,$(CPPFLAGS))"

# Additional dependencies for Python scripts.
sysdeps-linux-python-deps = \
  $(..)scripts/glibcextract.py \
  $(..)sysdeps/unix/sysv/linux/glibcsyscalls.py \
  # sysdeps-linux-python-deps

# Invocation of the Python interpreter with the Python search path.
sysdeps-linux-python = \
  PYTHONPATH=$(..)scripts:$(..)sysdeps/unix/sysv/linux $(PYTHON)

ifndef subdir
# This target performs two actions:
#
# Replace <arch-syscall.h> with a file generated from kernel headers
# and <fixup-asm-unistd.h>.  Both files are located via the sysdeps
# override search path.
#
# Update sysdeps/unix/sysv/linux/syscall-names.list with additional
# names found in the generated <arch-syscall.h> file, so that the
# global system call names list is a superset of the
# architecture-specific system call names.
#
# To bootstrap a new architecture, create an empty file in the right
# place and run `make update-syscall-lists' from the top-level of a
# configured, but not-yet-built glibc tree.
#
# --lock points to a file not replaced during the update operation, so
# that mutual exclusion is achieved.
.PHONY: update-syscall-lists
update-syscall-lists: arch-syscall.h
	$(sysdeps-linux-python) \
	  sysdeps/unix/sysv/linux/update-syscall-lists.py \
	    $(sysdeps-linux-python-cc) \
	    --lock=sysdeps/unix/sysv/linux/update-syscall-lists.py $< \
	    sysdeps/unix/sysv/linux/syscall-names.list
endif

ifeq ($(subdir),csu)
sysdep_routines += \
  errno-loc \
  # sysdep_routines
endif

ifeq ($(subdir),assert)
CFLAGS-assert.c += -DFATAL_PREPARE_INCLUDE='<fatal-prepare.h>'
CFLAGS-assert-perr.c += -DFATAL_PREPARE_INCLUDE='<fatal-prepare.h>'
endif

ifeq ($(subdir),malloc)
CFLAGS-malloc.c += -DMORECORE_CLEARS=2
endif

ifeq ($(subdir),misc)
sysdep_routines += \
  adjtimex \
  clock_adjtime \
  clone \
  clone-internal \
  clone-pidfd-support \
  clone3 \
  closefrom_fallback \
  convert_scm_timestamps \
  epoll_create \
  epoll_pwait \
  epoll_pwait2 \
  epoll_wait \
  eventfd \
  eventfd_read \
  eventfd_write \
  fanotify_mark \
  fxstat \
  fxstat64 \
  fxstatat \
  fxstatat64 \
  inotify_init \
  lxstat \
  lxstat64 \
  mlock2 \
  mremap \
  open_by_handle_at \
  personality \
  pkey_get \
  pkey_mprotect \
  pkey_set \
  prctl \
  prlimit \
  prlimit64 \
  process_vm_readv \
  process_vm_writev \
  pselect32 \
  readahead \
  setfsgid \
  setfsuid \
  setvmaname \
  signalfd \
  splice \
  sysctl \
  tee \
  timerfd_gettime \
  timerfd_settime \
  umount \
  umount2 \
  vmsplice \
  xmknod \
  xmknodat \
  xstat \
  xstat64 \
  # sysdep_routines

CFLAGS-gethostid.c = -fexceptions
CFLAGS-tee.c = -fexceptions -fasynchronous-unwind-tables
CFLAGS-vmsplice.c = -fexceptions -fasynchronous-unwind-tables
CFLAGS-splice.c = -fexceptions -fasynchronous-unwind-tables
CFLAGS-open_by_handle_at.c = -fexceptions -fasynchronous-unwind-tables
CFLAGS-sync_file_range.c = -fexceptions -fasynchronous-unwind-tables
CFLAGS-pselect32.c = -fexceptions -fasynchronous-unwind-tables
CFLAGS-tst-writev.c += "-DARTIFICIAL_LIMIT=(0x80000000-sysconf(_SC_PAGESIZE))"

# The test triggers FORTIFY abortion due to purposely wrong parameters
CFLAGS-test-errno-linux.c += $(no-fortify-source)

sysdep_headers += \
  bits/a.out.h \
  bits/epoll.h \
  bits/eventfd.h \
  bits/inotify.h \
  bits/mman-linux.h \
  bits/mman-map-flags-generic.h \
  bits/mman-shared.h \
  bits/procfs-extra.h \
  bits/procfs-id.h \
  bits/procfs-prregset.h \
  bits/procfs.h \
  bits/pthread_stack_min-dynamic.h \
  bits/pthread_stack_min.h \
  bits/ptrace-shared.h \
  bits/rseq.h \
  bits/shmlba.h \
  bits/siginfo-arch.h \
  bits/siginfo-consts-arch.h \
  bits/signalfd.h \
  bits/socket_type.h \
  bits/struct_stat.h \
  bits/struct_stat_time64_helper.h \
  bits/syscall.h \
  bits/termios-baud.h \
  bits/termios-c_cc.h \
  bits/termios-c_cflag.h \
  bits/termios-c_iflag.h \
  bits/termios-c_lflag.h \
  bits/termios-c_oflag.h \
  bits/termios-misc.h \
  bits/termios-struct.h \
  bits/termios-tcflow.h \
  bits/timerfd.h \
  bits/types/struct_msqid64_ds.h \
  bits/types/struct_msqid64_ds_helper.h \
  bits/types/struct_semid64_ds.h \
  bits/types/struct_semid64_ds_helper.h \
  bits/types/struct_semid_ds.h \
  bits/types/struct_shmid64_ds.h \
  bits/types/struct_shmid64_ds_helper.h \
  scsi/scsi.h \
  scsi/scsi_ioctl.h \
  scsi/sg.h \
  sys/acct.h \
  sys/epoll.h \
  sys/eventfd.h \
  sys/fanotify.h \
  sys/fsuid.h \
  sys/inotify.h \
  sys/kd.h \
  sys/klog.h \
  sys/mount.h \
  sys/pci.h \
  sys/personality.h \
  sys/pidfd.h \
  sys/prctl.h \
  sys/quota.h \
  sys/raw.h \
  sys/rseq.h \
  sys/signalfd.h \
  sys/soundcard.h \
  sys/timerfd.h \
  sys/user.h \
  sys/vt.h \
  # sysdep_headers

tests += \
  test-errno-linux \
  tst-adjtimex \
  tst-clock_adjtime \
  tst-clone \
  tst-clone2 \
  tst-clone3 \
  tst-epoll \
  tst-epoll-ioctls \
  tst-fanotify \
  tst-fdopendir-o_path \
  tst-getauxval \
  tst-gettid \
  tst-gettid-kill \
  tst-linux-mremap1 \
  tst-memfd_create \
  tst-misalign-clone \
  tst-mlock2 \
  tst-mount \
  tst-ntp_adjtime \
  tst-ntp_gettime \
  tst-ntp_gettimex \
  tst-ofdlocks \
  tst-personality \
  tst-pidfd \
  tst-pidfd_getpid \
  tst-pkey \
  tst-ppoll \
  tst-prctl \
  tst-process_mrelease \
  tst-quota \
  tst-rlimit-infinity \
  tst-scm_rights \
  tst-sigtimedwait \
  tst-sync_file_range \
  tst-syscall-restart \
  tst-sysconf-iov_max \
  tst-sysvmsg-linux \
  tst-sysvsem-linux \
  tst-sysvshm-linux \
  tst-tgkill \
  tst-timerfd \
  tst-ttyname-direct \
  tst-ttyname-namespace \
  # tests

# process_madvise requires CAP_SYS_ADMIN.
xtests += \
  tst-process_madvise \
  # xtests

# For +depfiles in Makerules.
extra-test-objs += \
  tst-sysconf-iov_max-uapi.o \
  # extra-test-objs

# Test for the symbol version of fcntl that was replaced in glibc 2.28.
ifeq ($(have-GLIBC_2.27)$(build-shared),yesyes)
tests += \
  tst-ofdlocks-compat \
  # tests
endif

tests-internal += \
  tst-rseq \
  tst-sigcontext-get_pc \
  # tests-internal

tests-internal += \
  tst-rseq-disable \
  # tests-internal

tests-time64 += \
  tst-adjtimex-time64 \
  tst-clock_adjtime-time64 \
  tst-epoll-time64 \
  tst-ntp_adjtime-time64 \
  tst-ntp_gettime-time64 \
  tst-ntp_gettimex-time64 \
  tst-ppoll-time64 \
  tst-prctl-time64 \
  tst-scm_rights-time64 \
  tst-sigtimedwait-time64 \
  tst-timerfd-time64 \
  # tests-time64

tests-clone-internal = \
  tst-align-clone-internal \
  tst-clone2-internal \
  tst-clone3-internal \
  tst-getpid1-internal
  # tests-clone-internal
tests-internal += \
  $(tests-clone-internal) \
  # tests-internal
tests-static += \
  $(tests-clone-internal) \
  # tests-static

CFLAGS-tst-sigcontext-get_pc.c = -fasynchronous-unwind-tables

# Generate the list of SYS_* macros for the system calls (__NR_*
# macros).  The file syscall-names.list contains all possible system
# call names, and the generated header file produces SYS_* macros for
# the __NR_* macros which are actually defined.

generated += bits/syscall.h
$(objpfx)bits/syscall.h: \
  ../sysdeps/unix/sysv/linux/gen-syscall-h.awk \
  ../sysdeps/unix/sysv/linux/syscall-names.list
	$(make-target-directory)
	LC_ALL=C $(AWK) -f $^ > $@-tmp
	$(move-if-change) $@-tmp $@
before-compile += $(objpfx)bits/syscall.h

# All macros defined by <sys/syscall.h>.  Include <bits/syscall.h>
# explicitly because <sys/sycall.h> skips it if _LIBC is defined.
$(objpfx)tst-syscall-list-macros.list: \
  $(objpfx)bits/syscall.h ../sysdeps/unix/sysv/linux/sys/syscall.h
	printf '#include <linux/version.h>\n\
#include <sys/syscall.h>\n#include <bits/syscall.h>\n' | \
	  $(CC) -E -o $@-tmp $(CFLAGS) $(CPPFLAGS) -x c - -dM
	$(move-if-change) $@-tmp $@

# __NR_* system call names.  Used by the test below.
$(objpfx)tst-syscall-list-nr.list: \
  ../sysdeps/unix/sysv/linux/filter-nr-syscalls.awk \
  $(objpfx)tst-syscall-list-macros.list
	LC_ALL=C $(AWK) -f $^ > $@-tmp
	$(move-if-change) $@-tmp $@

# SYS_* system call names.  Used by the test below.
$(objpfx)tst-syscall-list-sys.list: $(objpfx)tst-syscall-list-macros.list
	LC_ALL=C $(AWK) '/^#define SYS_/ { print substr($$2, 5) }' $< > $@-tmp
	$(move-if-change) $@-tmp $@

tests-special += \
  $(objpfx)tst-syscall-list.out
  # tests-special
$(objpfx)tst-syscall-list.out: \
  ../sysdeps/unix/sysv/linux/tst-syscall-list.sh \
  $(objpfx)tst-syscall-list-macros.list \
  $(objpfx)tst-syscall-list-nr.list \
  $(objpfx)tst-syscall-list-sys.list
	$(BASH) $^ $(AWK) > $@; $(evaluate-test)

tests-special += \
  $(objpfx)tst-glibcsyscalls.out
  # tests-special
# arch-syscall.h is located via the sysdeps override search path.
$(objpfx)tst-glibcsyscalls.out: arch-syscall.h \
  ../sysdeps/unix/sysv/linux/syscall-names.list
	$(sysdeps-linux-python) \
	  ../sysdeps/unix/sysv/linux/tst-glibcsyscalls.py \
	    $(sysdeps-linux-python-cc) $< \
	    ../sysdeps/unix/sysv/linux/syscall-names.list \
	  < /dev/null > $@ 2>&1; $(evaluate-test)
$(objpfx)tst-glibcsyscalls.out: \
  ../sysdeps/unix/sysv/linux/tst-glibcsyscalls.py \
  $(sysdeps-linux-python-deps)

# Separate object file for access to the constant from the UAPI header.
$(objpfx)tst-sysconf-iov_max: $(objpfx)tst-sysconf-iov_max-uapi.o

tests-special += \
  $(objpfx)tst-mman-consts.out \
  # tests-special
$(objpfx)tst-mman-consts.out: ../sysdeps/unix/sysv/linux/tst-mman-consts.py
	$(sysdeps-linux-python) \
	  ../sysdeps/unix/sysv/linux/tst-mman-consts.py \
	    $(sysdeps-linux-python-cc) \
	  < /dev/null > $@ 2>&1; $(evaluate-test)
$(objpfx)tst-mman-consts.out: $(sysdeps-linux-python-deps)

tests-special += \
  $(objpfx)tst-pidfd-consts.out \
  # tests-special
$(objpfx)tst-pidfd-consts.out: ../sysdeps/unix/sysv/linux/tst-pidfd-consts.py
	$(sysdeps-linux-python) \
	  ../sysdeps/unix/sysv/linux/tst-pidfd-consts.py \
	    $(sysdeps-linux-python-cc) \
	  < /dev/null > $@ 2>&1; $(evaluate-test)
$(objpfx)tst-pidfd-consts.out: $(sysdeps-linux-python-deps)

tests-special += \
  $(objpfx)tst-mount-consts.out \
  # tests-special
$(objpfx)tst-mount-consts.out: ../sysdeps/unix/sysv/linux/tst-mount-consts.py
	$(sysdeps-linux-python) \
	  ../sysdeps/unix/sysv/linux/tst-mount-consts.py \
	    $(sysdeps-linux-python-cc) \
	  < /dev/null > $@ 2>&1; $(evaluate-test)
$(objpfx)tst-mount-consts.out: $(sysdeps-linux-python-deps)

tests-special += \
  $(objpfx)tst-mount-compile.out \
  # tests-special
$(objpfx)tst-mount-compile.out: ../sysdeps/unix/sysv/linux/tst-mount-compile.py
	$(sysdeps-linux-python) \
	  ../sysdeps/unix/sysv/linux/tst-mount-compile.py \
	    $(sysdeps-linux-python-cc) \
	  < /dev/null > $@ 2>&1; $(evaluate-test)
$(objpfx)tst-mount-compile.out: $(sysdeps-linux-python-deps)

tst-rseq-disable-ENV = GLIBC_TUNABLES=glibc.pthread.rseq=0

endif # $(subdir) == misc

ifeq ($(subdir),time)
sysdep_headers += \
  bits/timex.h \
  sys/timex.h \
  # sysdep_headers

sysdep_routines += \
  ntp_gettime \
  ntp_gettimex \
  # sysdep_routines

tests += \
  tst-clock_gettime-clobber \
  tst-gettimeofday-clobber \
  tst-time-clobber \
  # tests
endif

ifeq ($(subdir),signal)
tests-special += \
  $(objpfx)tst-signal-numbers.out \
  # tests-special

# Depending on signal.o* is a hack.  What we actually want is a dependency
# on signal.h and everything it includes.  That's impractical to write
# in this context, but signal.c includes signal.h and not much else so it'll
# be conservatively correct.
$(objpfx)tst-signal-numbers.out: \
		../sysdeps/unix/sysv/linux/tst-signal-numbers.py \
		$(objpfx)signal.o*
	$(sysdeps-linux-python) \
	  ../sysdeps/unix/sysv/linux/tst-signal-numbers.py \
	    $(sysdeps-linux-python-cc) \
	  < /dev/null > $@ 2>&1; $(evaluate-test)
$(objpfx)tst-signal-numbers.out: $(sysdeps-linux-python-deps)
endif

ifeq ($(subdir),socket)
sysdep_headers += \
  bits/socket-constants.h \
  net/ethernet.h \
  net/if_arp.h \
  net/if_packet.h \
  net/if_ppp.h \
  net/if_shaper.h \
  net/if_slip.h \
  net/ppp-comp.h \
  net/ppp_defs.h \
  net/route.h \
  # sysdep_headers

sysdep_routines += \
  cmsg_nxthdr \
  # sysdep_routines

CFLAGS-recvmmsg.c = -fexceptions -fasynchronous-unwind-tables
CFLAGS-sendmmsg.c = -fexceptions -fasynchronous-unwind-tables

tests += \
  tst-socket-timestamp \
  tst-socket-timestamp-compat \
  # tests
tests-time64 += \
  tst-socket-timestamp-compat-time64 \
  tst-socket-timestamp-time64 \
  # tests-time64

tests-special += \
  $(objpfx)tst-socket-consts.out \
  # tests-special
$(objpfx)tst-socket-consts.out: ../sysdeps/unix/sysv/linux/tst-socket-consts.py
	PYTHONPATH=../scripts \
	$(PYTHON) ../sysdeps/unix/sysv/linux/tst-socket-consts.py \
		   --cc="$(CC) $(patsubst -DMODULE_NAME=%, \
					  -DMODULE_NAME=testsuite, \
					  $(CPPFLAGS)) -D_ISOMAC" \
	< /dev/null > $@ 2>&1; $(evaluate-test)
endif # $(subdir) == socket

ifeq ($(subdir),sunrpc)
sysdep_headers += \
  nfs/nfs.h \
  # sysdep_headers
endif

ifeq ($(subdir),termios)
sysdep_headers += \
  termio.h \
  # sysdep_headers
endif

ifeq ($(subdir),posix)
sysdep_headers += \
  bits/initspin.h \
  # sysdep_headers

sysdep_routines += \
  getcpu \
  oldglob \
  pidfd_getpid \
  pidfd_spawn \
  pidfd_spawnp \
  procutils \
  sched_getcpu \
  spawnattr_getcgroup_np \
  spawnattr_setcgroup_np \
  # sysdep_routines

tests += \
  tst-affinity \
  tst-affinity-pid \
  tst-posix_spawn-setsid-pidfd \
  tst-spawn-cgroup \
  tst-spawn-chdir-pidfd \
  tst-spawn-pidfd \
  tst-spawn2-pidfd \
  tst-spawn3-pidfd \
  tst-spawn4-pidfd \
  tst-spawn5-pidfd \
  tst-spawn6-pidfd \
  tst-spawn7-pidfd \
  # tests

tests-static += \
  tst-affinity-static \
  # tests-static

tests += \
  $(tests-static) \
  # tests

CFLAGS-fork.c = $(libio-mtsafe)
CFLAGS-getpid.o = -fomit-frame-pointer
CFLAGS-getpid.os = -fomit-frame-pointer
CFLAGS-tst-spawn3-pidfd.c += -DOBJPFX=\"$(objpfx)\"

tst-spawn-cgroup-ARGS = -- $(host-test-program-cmd)
tst-spawn-pidfd-ARGS = -- $(host-test-program-cmd)
tst-spawn5-pidfd-ARGS = -- $(host-test-program-cmd)
tst-spawn6-pidfd-ARGS = -- $(host-test-program-cmd)
tst-spawn7-pidfd-ARGS = -- $(host-test-program-cmd)
tst-posix_spawn-setsid-pidfd-ARGS = -- $(host-test-program-cmd)
endif

ifeq ($(subdir),inet)
sysdep_headers += \
  netash/ash.h \
  netatalk/at.h \
  netax25/ax25.h \
  neteconet/ec.h \
  netinet/if_fddi.h \
  netinet/if_tr.h \
  netipx/ipx.h \
  netiucv/iucv.h \
  netpacket/packet.h \
  netrom/netrom.h \
  netrose/rose.h \
  # sysdep_headers

sysdep_routines += \
  netlink_assert_response \
  # sysdep_routines

CFLAGS-check_pf.c += -fexceptions
endif

# Don't compile the ctype glue code, since there is no old non-GNU C library.
inhibit-glue = yes

ifeq ($(subdir),dirent)
sysdep_routines += \
  getdirentries \
  getdirentries64 \
  # sysdep_routines

tests += \
  tst-getdents64 \
  tst-readdir64-compat \
  # tests
endif # $(subdir) == dirent

ifeq ($(subdir),nis)
CFLAGS-ypclnt.c = -DUSE_BINDINGDIR=1
endif

ifeq ($(subdir),io)
sysdep_routines += \
  close_nocancel \
  fallocate \
  fallocate64 \
  fcntl_nocancel \
  internal_statvfs \
  open64_nocancel \
  open_nocancel \
  openat64_nocancel \
  openat_nocancel \
  pread64_nocancel \
  read_nocancel \
  stat_t64_cp \
  statx_cp \
  sync_file_range \
  write_nocancel \
  xstatconv \
  # sysdep_routines

sysdep_headers += \
  bits/fcntl-linux.h \
  # sysdep_headers

tests += \
  tst-fallocate \
  tst-fallocate64 \
  tst-getcwd-smallbuff \
  tst-o_path-locks \
  # tests
endif

ifeq ($(subdir),elf)
dl-routines += \
  dl-rseq-symbols \
  # dl-routines

sysdep-rtld-routines += \
  dl-brk \
  dl-getcwd \
  dl-openat64 \
  dl-opendir \
  dl-sbrk \
  # sysdep-rtld-routines

others += \
  pldd \
  # others

install-bin += \
  pldd \
  # install-bin

$(objpfx)pldd: $(objpfx)xmalloc.o
endif

ifeq ($(subdir),rt)
CFLAGS-mq_send.c += -fexceptions
CFLAGS-mq_receive.c += -fexceptions
endif

ifeq ($(subdir),nscd)
sysdep-CFLAGS += -DHAVE_EPOLL -DHAVE_INOTIFY -DHAVE_NETLINK
CFLAGS-gai.c += -DNEED_NETLINK
endif

ifeq ($(subdir),nptl)
tests += \
  tst-align-clone \
  tst-getpid1 \
  # tests

# tst-rseq-nptl is an internal test because it requires a definition of
# __NR_rseq from the internal system call list.
tests-internal += \
  tst-rseq-nptl \
  # tests-internal
endif
