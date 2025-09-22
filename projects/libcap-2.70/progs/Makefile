
topdir=$(shell pwd)/..
include $(topdir)/Make.Rules
#
# Programs: all of the examples that we will compile
#
PROGS=getpcaps getcap setcap

BUILD=$(PROGS)

all: $(BUILD) capsh

ifeq ($(DYNAMIC),yes)
LDPATH = LD_LIBRARY_PATH=../libcap
DEPS = ../libcap/libcap.so
else
ifeq ($(LIBCSTATIC),yes)
LDFLAGS = --static
DEPS = ../libcap/libcap.a
else
# For this build variant override the LDFLAGS to link statically from
# libraries within the build tree. If you never want this, use make
# DYNAMIC=yes . Note, we can't reliably link statically against glibc
# becasuse of https://sourceware.org/bugzilla/show_bug.cgi?id=12491 .
LDFLAGS = -Wl,-Bstatic
LDFLAGS_SUFFIX = -Wl,-Bdynamic
DEPS = ../libcap/libcap.a
endif
endif

../libcap/libcap.a:
	$(MAKE) -C ../libcap libcap.a

../libcap/libcap.so:
	$(MAKE) -C ../libcap libcap.so

$(BUILD): %: %.o $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LIBCAPLIB) $(LDFLAGS_SUFFIX) -o $@

%.o: %.c $(INCS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

install: all
	mkdir -p -m 0755 $(FAKEROOT)$(SBINDIR)
	for p in $(PROGS) capsh ; do \
		install -m 0755 $$p $(FAKEROOT)$(SBINDIR) ; \
	done
ifeq ($(RAISE_SETFCAP),yes)
	$(FAKEROOT)$(SBINDIR)/setcap cap_setfcap=i $(FAKEROOT)$(SBINDIR)/setcap
endif

test:
	@echo "no program tests without privilege, try 'make sudotest'"

capshdoc.c.cf: capshdoc.c ./mkcapshdoc.sh
	./mkcapshdoc.sh > $@
	diff -u capshdoc.c $@ || (rm $@ ; exit 1)

capsh: capsh.c capshdoc.c.cf capshdoc.h $(DEPS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(CAPSH_SHELL) $(LDFLAGS) $< capshdoc.c $(LIBCAPLIB) $(LDFLAGS_SUFFIX) -o $@

# Statically linked with minimal linkage flags to enable running in a
# chroot and in other in-tree testing contexts.
tcapsh-static: capsh.c capshdoc.c.cf capshdoc.h $(DEPS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(CAPSH_SHELL) -o $@ $< capshdoc.c $(LIBCAPLIB) --static

uns_test: ../tests/uns_test.c
	$(MAKE) -C ../tests uns_test
	cp ../tests/uns_test .

sudotest: tcapsh-static uns_test capsh setcap getcap getpcaps tcapsh-static
	$(SUDO) $(LDPATH) ./quicktest.sh

clean:
	$(LOCALCLEAN)
	rm -f *.o $(BUILD) privileged ping hack.sh compare-cap uns_test
	rm -f capsh tcapsh* capshdoc.*.cf
