#
# NOTE the built tests are all designed to be run from this
# working directory when built DYNAMIC=yes. That is, they
# link to the shared libraries in ../libcap/ .
#
topdir=$(shell pwd)/..
include ../Make.Rules
#

all:
	@echo leave test building to test target

install:
	@echo nothing to install from tests

ifeq ($(DYNAMIC),yes)
LINKEXTRA=-Wl,-rpath,../libcap
DEPS=../libcap/libcap.so
ifeq ($(PTHREADS),yes)
DEPS += ../libcap/libpsx.so
endif
else
# For this build variant override the LDFLAGS to link statically from
# libraries within the build tree. If you never want this, use
# make DYNAMIC=yes ...
LDFLAGS = --static
DEPS=../libcap/libcap.a
ifeq ($(PTHREADS),yes)
DEPS +=  ../libcap/libpsx.a
endif
endif

../libcap/libcap.so:
	$(MAKE) -C ../libcap libcap.so

../libcap/libcap.a:
	$(MAKE) -C ../libcap libcap.a

ifeq ($(PTHREADS),yes)
../libcap/libpsx.so:
	$(MAKE) -C ../libcap libpsx.so

../libcap/libpsx.a:
	$(MAKE) -C ../libcap libpsx.a
endif

../progs/tcapsh-static:
	$(MAKE) -C ../progs tcapsh-static

test:
ifeq ($(PTHREADS),yes)
	$(MAKE) run_psx_test run_libcap_psx_test
endif

sudotest: test
	$(MAKE) run_uns_test
	$(MAKE) run_libcap_launch_test
ifeq ($(PTHREADS),yes)
	$(MAKE) run_libcap_psx_launch_test run_exploit_test
endif

# unprivileged
run_psx_test: psx_test
	./psx_test

psx_test: psx_test.c $(DEPS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $< -o $@ $(LINKEXTRA) $(LIBPSXLIB)

run_libcap_psx_test: libcap_psx_test
	./libcap_psx_test

libcap_psx_test: libcap_psx_test.c $(DEPS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $< -o $@ $(LINKEXTRA) $(LIBCAPLIB) $(LIBPSXLIB)

# privileged
uns_test: uns_test.c $(DEPS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $< -o $@ $(LINKEXTRA) $(LIBCAPLIB)

run_uns_test: uns_test
	echo exit | $(SUDO) ./uns_test

run_libcap_launch_test: libcap_launch_test noop ../progs/tcapsh-static
	$(SUDO) ./libcap_launch_test

run_libcap_psx_launch_test: libcap_psx_launch_test ../progs/tcapsh-static
	$(SUDO) ./libcap_psx_launch_test

libcap_launch_test: libcap_launch_test.c $(DEPS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $< -o $@ $(LINKEXTRA) $(LIBCAPLIB)

# This varies only slightly from the above insofar as it currently
# only links in the pthreads fork support. TODO() we need to change
# the source to do something interesting with pthreads.
libcap_psx_launch_test: libcap_launch_test.c $(DEPS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -DWITH_PTHREADS $< -o $@ $(LINKEXTRA) $(LIBCAPLIB) $(LIBPSXLIB)


# This test demonstrates that libpsx is needed to secure multithreaded
# programs that link against libcap.
run_exploit_test: exploit noexploit
	@echo exploit should succeed
	$(SUDO) ./exploit ; if [ $$? -ne 0 ]; then exit 0; else exit 1 ; fi
	@echo exploit should fail
	$(SUDO) ./noexploit ; if [ $$? -eq 0 ]; then exit 0; else exit 1 ; fi

exploit: exploit.o $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@ $(LINKEXTRA) $(LIBCAPLIB) -lpthread

# Note, for some reason, the order of libraries is important to avoid
# the exploit working for dynamic linking.
noexploit: exploit.o $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@ $(LINKEXTRA) $(LIBPSXLIB) $(LIBCAPLIB)

# This one runs in a chroot with no shared library files.
noop: noop.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -o $@ --static

clean:
	rm -f psx_test libcap_psx_test libcap_launch_test uns_test *~
	rm -f libcap_launch_test libcap_psx_launch_test core noop
	rm -f exploit noexploit exploit.o
