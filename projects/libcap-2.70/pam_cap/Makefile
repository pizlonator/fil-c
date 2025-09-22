# simple make file for the pam_cap module

topdir=$(shell pwd)/..
include ../Make.Rules

# Always build pam_cap sources this way:
CFLAGS += -fPIC

all: pam_cap.so
	$(MAKE) testlink

install: all
	mkdir -p -m 0755 $(FAKEROOT)$(LIBDIR)/security
	install -m 0755 pam_cap.so $(FAKEROOT)$(LIBDIR)/security

../libcap/loader.txt:
	$(MAKE) -C ../libcap loader.txt

execable.o: execable.c ../libcap/execable.h ../libcap/loader.txt
	$(CC) $(CFLAGS) $(CPPFLAGS) -DLIBCAP_VERSION=\"libcap-$(VERSION).$(MINOR)\" -DSHARED_LOADER=\"$(shell cat ../libcap/loader.txt)\" -c execable.c -o $@

LIBCAP:
	$(MAKE) -C ../libcap all
	touch $@

pam_cap.so: pam_cap.o execable.o pam_cap_linkopts LIBCAP
	cat pam_cap_linkopts | xargs -e $(LD) $(LDFLAGS) -o $@ pam_cap.o execable.o $(LIBCAPLIB)

# Some distributions force link everything at compile time, and don't
# take advantage of libpam's dlopen runtime options to resolve ill
# defined symbols from its own linkage as needed. (As the original
# author of that part of libpam, I consider this force linking
# premature optimization.) We debugged its consequences to pam_cap.so
# as part of:
#
#   https://bugzilla.kernel.org/show_bug.cgi?id=214023
#
# If the current build environment is one of those, or we can't
# reliably prove it isn't, extend the link options for pam_cap.so to
# force linkage against libpam and the gazillion other things libpam
# is linked against...
#
# If you want to force this behavior one way or the other, use the
# make FORCELINKPAM=yes or FORCELINKPAM=no override.
ifeq ($(FORCELINKPAM),yes)
pam_cap_linkopts: Makefile
	echo "-Wl,-e,__so_start -lpam" > $@
else
ifeq ($(FORCELINKPAM),no)
pam_cap_linkopts: Makefile
	echo "-Wl,-e,__so_start" > $@
else
pam_cap_linkopts: lazylink.so
	echo "-Wl,-e,__so_start" > $@
	./lazylink.so || echo "-lpam" >> $@

lazylink.so: lazylink.c ../libcap/execable.h ../libcap/loader.txt
	$(LD) -o $@ $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) lazylink.c -DSHARED_LOADER=\"$(shell cat ../libcap/loader.txt)\" -Wl,-e,__so_start
endif
endif

../libcap/libcap.a:
	$(MAKE) -C ../libcap libcap.a

# Avoid $(LDFLAGS) here to avoid conflicts with --static for a in-tree
# test binary.
test_pam_cap: test_pam_cap.c pam_cap.c ../libcap/libcap.a
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_pam_cap.c $(LIBCAPLIB) --static

testlink: test.o pam_cap.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $+ -lpam -ldl $(LIBCAPLIB)

incapable.conf:
	echo "^cap_setuid  alpha" > $@ && chmod o+w $@

test: testlink test_pam_cap pam_cap.so incapable.conf
	./test_pam_cap
	LD_LIBRARY_PATH=../libcap ./pam_cap.so
	LD_LIBRARY_PATH=../libcap ./pam_cap.so --help
	@echo "module can be run as an executable!"

sudotest: test_pam_cap incapable.conf
	$(SUDO) ./test_pam_cap root 0x0 0x0 0x0 config=./capability.conf
	$(SUDO) ./test_pam_cap root 0x0 0x0 0x0 config=./sudotest.conf
	$(SUDO) ./test_pam_cap alpha 0x0 0x0 0x0 config=./capability.conf
	$(SUDO) ./test_pam_cap alpha 0x0 0x1 0x80 config=./sudotest.conf
	$(SUDO) ./test_pam_cap beta 0x0 0x1 0x0 config=./sudotest.conf
	$(SUDO) ./test_pam_cap gamma 0x0 0x0 0x81 config=./sudotest.conf
	$(SUDO) ./test_pam_cap delta 0x41 0x80 0x41 config=./sudotest.conf

clean:
	rm -f *.o *.so testlink lazylink.so test_pam_cap pam_cap_linkopts *~
	rm -f LIBCAP incapable.conf
