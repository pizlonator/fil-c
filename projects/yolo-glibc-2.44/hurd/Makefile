# Copyright (C) 1991-2026 Free Software Foundation, Inc.
# This file is part of the GNU C Library.

# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.

subdir := hurd

include ../Makeconfig

tests := test-sig-xstate \
	test-sig-rpc-interrupted
$(objpfx)test-sig-xstate: $(shared-thread-library)
$(objpfx)test-sig-rpc-interrupted: $(shared-thread-library) $(objdir)/hurd/libhurduser.so

headers = \
  $(interface-headers) \
  hurd.h \
  hurd/fd.h \
  hurd/id.h \
  hurd/ioctl.h \
  hurd/lookup.h \
  hurd/port.h \
  hurd/resource.h \
  hurd/signal.h \
  hurd/sigpreempt.h \
  hurd/userlink.h \
  # headers

inline-headers = \
  hurd.h \
  hurd/fd.h \
  hurd/port.h \
  hurd/signal.h \
  hurd/userlink.h \
  # inline-headers

# The RPC interfaces go in a separate library.
interface-library := libhurduser
user-interfaces := \
  hurd/auth \
  hurd/crash \
  hurd/exec \
  hurd/exec_startup \
  hurd/fs \
  hurd/fsys \
  hurd/ifsock \
  hurd/interrupt \
  hurd/io \
  hurd/io_reply \
  hurd/io_request \
  hurd/login \
  hurd/msg \
  hurd/msg_reply \
  hurd/msg_request \
  hurd/password \
  hurd/pci \
  hurd/pfinet \
  hurd/process \
  hurd/process_request \
  hurd/socket \
  hurd/startup \
  hurd/term \
  hurd/tioctl \
  # user-interfaces

server-interfaces := \
  faultexc \
  hurd/msg \
  # server-interfaces

routines = \
  $(dtable) \
  $(inlines) \
  $(sig) \
  errno-loc \
  fchroot \
  fd-cleanup \
  fopenport \
  get-host \
  geteuids \
  getumask \
  hurdauth \
  hurdchdir \
  hurdexec \
  hurdfchdir \
  hurdid \
  hurdinit \
  hurdlock \
  hurdlookup \
  hurdmsg \
  hurdpid \
  hurdports \
  hurdprio \
  hurdrlimit \
  hurdselect \
  hurdsock \
  hurdstartup \
  lookup-at \
  lookup-retry \
  msgportdemux \
  path-lookup \
  pid2task \
  port-cleanup \
  ports-get \
  ports-set \
  privports \
  report-wait \
  set-host \
  setauth \
  seteuids \
  task2pid \
  vpprintf \
  xattr \
  # routines
sig = \
  catch-exc \
  catch-signal \
  exc2signal \
  hurd-raise \
  hurdfault \
  hurdkill \
  hurdsig \
  intr-msg \
  longjmp-ts \
  preempt-sig \
  siginfo \
  sigthreadmask \
  sigunwind \
  thread-cancel \
  thread-self \
  trampoline \
  # sig
dtable = \
  alloc-fd \
  ctty-input \
  ctty-output \
  dtable \
  fd-close \
  fd-read \
  fd-write \
  getdport \
  hurdioctl \
  intern-fd \
  new-fd \
  openport \
  port2fd \
  # dtable
inlines = $(inline-headers:%.h=%-inlines)

# XXX this is a temporary hack; see hurdmalloc.h
routines += hurdmalloc

# Binary compatibility for libc.so.0.2[GLIBC_2.0].
ifeq ($(build-shared),yes)
routines += compat-20
endif

shared-only-routines = compat-20

# For each of the $(inline-headers), generate a trivial source
# file that will #include it to define its inline functions as real functions.
$(inlines:%=$(objpfx)%.c): $(objpfx)%-inlines.c: %.h
	(h="`echo $(subst /,_,$*) | tr '[a-z]' '[A-Z]'`"; \
	 echo "#define _$${h}_H_EXTERN_INLINE /* Define real function.  */"; \
	 echo "#define _$${h}_H_HIDDEN_DEF    /* Declare hidden definition.  */"; \
	 echo '#include "$<"') > $@-new
	mv -f $@-new $@
generated += $(inlines:=.c)

# Avoid ssp before TLS is initialized.
CFLAGS-hurdstartup.o = $(no-stack-protector)
CFLAGS-RPC_exec_startup_get_info.o = $(no-stack-protector)

# Make it simpler to unwind over INTR_MSG_TRAP
CFLAGS-intr-msg.o = -fno-omit-frame-pointer
CFLAGS-intr-msg.os = -fno-omit-frame-pointer

# Make generated headers compatible with all support standards
migheaderpipe := | sed -e 's/\<ino64_t\>/__ino64_t/' -e 's/\<loff_t\>/__loff_t/'
include ../mach/Machrules
libhurduser-routines += RPC_gsync_wait_intr
include ../Rules

# intr-rpc.defs defines the INTR_INTERFACE macro to make the generated RPC
# stubs import <hurd/signal.h> and #define __mach_msg to
# _hurd_intr_rpc_mach_msg.
user-MIGFLAGS += -imacros intr-rpc.defs

# The special exc server for sigthread faults uses a special prefix.
MIGFLAGS-faultexc = -prefix _hurdsig_fault_

# We need this static dependency to get faultexc.h generated the first time.
$(objpfx)hurdfault.o $(objpfx)hurdfault.d: \
	$(objpfx)faultexc_server.h $(objpfx)faultexc_server.c
