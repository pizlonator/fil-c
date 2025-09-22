#
# defines
#
topdir=$(shell pwd)/..
include ../Make.Rules
#
# Library version
#
CAPLIBNAME=$(LIBTITLE).so
STACAPLIBNAME=$(LIBTITLE).a
#
PSXTITLE=libpsx
PSXLIBNAME=$(PSXTITLE).so
STAPSXLIBNAME=$(PSXTITLE).a

CAPFILES=cap_alloc cap_proc cap_extint cap_flag cap_text cap_file
CAPMAGICOBJ=cap_magic.o
PSXFILES=../psx/psx
PSXMAGICOBJ=psx_magic.o

# Always build libcap sources this way:
CFLAGS += -fPIC

# The linker magic needed to build a dynamic library as independently
# executable
MAGIC=-Wl,-e,__so_start

INCLS=libcap.h cap_names.h $(INCS)
GPERF_OUTPUT = _caps_output.gperf

CAPOBJS=$(addsuffix .o, $(CAPFILES))
MAJCAPLIBNAME=$(CAPLIBNAME).$(VERSION)
MINCAPLIBNAME=$(MAJCAPLIBNAME).$(MINOR)

PSXOBJS=$(addsuffix .o, $(PSXFILES))
MAJPSXLIBNAME=$(PSXLIBNAME).$(VERSION)
MINPSXLIBNAME=$(MAJPSXLIBNAME).$(MINOR)

all: pcs $(STACAPLIBNAME)
ifeq ($(SHARED),yes)
	$(MAKE) $(CAPLIBNAME)
endif
ifeq ($(PTHREADS),yes)
	$(MAKE) $(STAPSXLIBNAME)
ifeq ($(SHARED),yes)
	$(MAKE) $(PSXLIBNAME)
endif
endif

pcs: $(LIBTITLE).pc
ifeq ($(PTHREADS),yes)
	$(MAKE) $(PSXTITLE).pc
endif

ifeq ($(USE_GPERF),yes)
USE_GPERF_OUTPUT = $(GPERF_OUTPUT)
INCLUDE_GPERF_OUTPUT = -DINCLUDE_GPERF_OUTPUT='"$(GPERF_OUTPUT)"'
endif

$(LIBTITLE).pc: $(LIBTITLE).pc.in
	$(BUILD_SED) -e 's,@prefix@,$(prefix),' \
		-e 's,@exec_prefix@,$(exec_prefix),' \
		-e 's,@libdir@,$(LIBDIR),' \
		-e 's,@includedir@,$(inc_prefix)/include,' \
		-e 's,@VERSION@,$(VERSION).$(MINOR),' \
		-e 's,@deps@,$(DEPS),' \
		$< >$@

$(PSXTITLE).pc: $(PSXTITLE).pc.in
	$(BUILD_SED) -e 's,@prefix@,$(prefix),' \
		-e 's,@exec_prefix@,$(exec_prefix),' \
		-e 's,@libdir@,$(LIBDIR),' \
		-e 's,@includedir@,$(inc_prefix)/include,' \
		-e 's,@VERSION@,$(VERSION).$(MINOR),' \
		-e 's,@deps@,$(DEPS),' \
		$< >$@

_makenames: _makenames.c cap_names.list.h
	$(BUILD_CC) $(BUILD_CFLAGS) $(BUILD_CPPFLAGS) $< -o $@

cap_names.h: _makenames
	./_makenames > cap_names.h

$(GPERF_OUTPUT): cap_names.list.h cap_names.header Makefile
	(cat cap_names.header ; $(BUILD_SED) -e 's/[\{\}"]//g' -e 's/,$$//' cap_names.list.h) | gperf --ignore-case --language=ANSI-C --readonly --null-strings --global-table --hash-function-name=__cap_hash_name --lookup-function-name="__cap_lookup_name" -c -t -m20 $(INDENT) > $@
	$(BUILD_SED) -e 's/unsigned int len/size_t len/' -i $@

# Intention is that libcap keeps up with torvalds' tree, as reflected
# by this maintained version of the kernel header. libcap dynamically
# trims the meaning of "all" capabilities down to that of the running
# kernel as of 2.30. That is, all production kernels should be equal
# to or behind libcap.
#
# Note "./libcap.so --summary" should explain how the built libcap.so
# compares to the running kernel.
UAPI_HEADER := $(topdir)/libcap/include/uapi/linux/capability.h
cap_names.list.h: Makefile $(UAPI_HEADER)
	@echo "=> making $@ from $(UAPI_HEADER)"
	$(BUILD_EGREP) '^#define\s+CAP_([^\s]+)\s+[0-9]+\s*$$' include/uapi/linux/capability.h | $(BUILD_SED) -e 's/^#define\s\+/{"/' -e 's/\s*$$/},/' -e 's/\s\+/",/' -e 'y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/' > $@

$(STACAPLIBNAME): $(CAPOBJS)
	$(AR) rcs $@ $^
	$(RANLIB) $@

$(STAPSXLIBNAME): $(PSXOBJS) include/sys/psx_syscall.h
	$(AR) rcs $@ $(PSXOBJS)
	$(RANLIB) $@

ifeq ($(SHARED),yes)

empty: empty.c
	$(CC) -o $@ $<

loader.txt: empty
	$(OBJCOPY) --dump-section .interp=$@ $< /dev/null

cap_magic.o: execable.h execable.c loader.txt libcap.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -DLIBRARY_VERSION=\"$(LIBTITLE)-$(VERSION).$(MINOR)\" -DSHARED_LOADER=\"$(shell cat loader.txt)\" -include ./libcap.h -c execable.c -o $@

$(CAPLIBNAME) $(MAJCAPLIBNAME) $(MINCAPLIBNAME): $(CAPOBJS) $(CAPMAGICOBJ)
	$(LD) $(CFLAGS) $(LDFLAGS) -Wl,-soname,$(MAJCAPLIBNAME) -o $(MINCAPLIBNAME) $^ $(MAGIC)
	ln -sf $(MINCAPLIBNAME) $(MAJCAPLIBNAME)
	ln -sf $(MAJCAPLIBNAME) $(CAPLIBNAME)

psx_magic.o: execable.h psx_exec.c loader.txt
	$(CC) $(CFLAGS) $(CPPFLAGS) -DLIBRARY_VERSION=\"$(PSXTITLE)-$(VERSION).$(MINOR)\" -DSHARED_LOADER=\"$(shell cat loader.txt)\" -c psx_exec.c -o $@

$(PSXLIBNAME) $(MAJPSXLIBNAME) $(MINPSXLIBNAME): $(PSXOBJS) include/sys/psx_syscall.h $(PSXMAGICOBJ)
	$(LD) $(CFLAGS) $(LDFLAGS) -Wl,-soname,$(MAJPSXLIBNAME) -o $(MINPSXLIBNAME) $(PSXOBJS) $(PSXMAGICOBJ) $(MAGIC) $(PSXLINKFLAGS)
	ln -sf $(MINPSXLIBNAME) $(MAJPSXLIBNAME)
	ln -sf $(MAJPSXLIBNAME) $(PSXLIBNAME)
endif

%.o: %.c $(INCLS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

cap_text.o: cap_text.c $(USE_GPERF_OUTPUT) $(INCLS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(INCLUDE_GPERF_OUTPUT) -c $< -o $@

cap_test: cap_test.c $(INCLS) $(CAPOBJS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $< $(CAPOBJS) -o $@

libcapsotest: $(CAPLIBNAME)
	./$(CAPLIBNAME)
	./$(CAPLIBNAME) --usage
	./$(CAPLIBNAME) --help
	./$(CAPLIBNAME) --summary

libpsxsotest: $(PSXLIBNAME)
	./$(PSXLIBNAME)

test: cap_test
	./cap_test
ifeq ($(SHARED),yes)
	$(MAKE) libcapsotest
ifeq ($(PTHREADS),yes)
	$(MAKE) libpsxsotest
endif
endif

sudotest:
	@echo no sudotests for libcap

install: install-static
ifeq ($(SHARED),yes)
	$(MAKE) install-shared
endif

install-static: install-static-cap
ifeq ($(PTHREADS),yes)
	$(MAKE) install-static-psx
endif

install-shared: install-shared-cap
ifeq ($(PTHREADS),yes)
	$(MAKE) install-shared-psx
endif

install-cap: install-static-cap
ifeq ($(SHARED),yes)
	$(MAKE) install-shared-cap
endif

install-psx: install-static-psx
ifeq ($(SHARED),yes)
	$(MAKE) install-shared-psx
endif

install-static-cap: install-common-cap $(STACAPLIBNAME)
	install -m 0644 $(STACAPLIBNAME) $(FAKEROOT)$(LIBDIR)/$(STACAPLIBNAME)

install-shared-cap: install-common-cap $(MINCAPLIBNAME)
	install -m 0755 $(MINCAPLIBNAME) $(FAKEROOT)$(LIBDIR)/$(MINCAPLIBNAME)
	ln -sf $(MINCAPLIBNAME) $(FAKEROOT)$(LIBDIR)/$(MAJCAPLIBNAME)
	ln -sf $(MAJCAPLIBNAME) $(FAKEROOT)$(LIBDIR)/$(CAPLIBNAME)
ifeq ($(FAKEROOT),)
	-/sbin/ldconfig
endif

install-static-psx: install-common-psx $(STAPSXLIBNAME)
	install -m 0644 $(STAPSXLIBNAME) $(FAKEROOT)$(LIBDIR)/$(STAPSXLIBNAME)

install-shared-psx: install-common-psx $(MINPSXLIBNAME)
	install -m 0755 $(MINPSXLIBNAME) $(FAKEROOT)$(LIBDIR)/$(MINPSXLIBNAME)
	ln -sf $(MINPSXLIBNAME) $(FAKEROOT)$(LIBDIR)/$(MAJPSXLIBNAME)
	ln -sf $(MAJPSXLIBNAME) $(FAKEROOT)$(LIBDIR)/$(PSXLIBNAME)
ifeq ($(FAKEROOT),)
	-/sbin/ldconfig
endif

install-common-cap: install-common $(LIBTITLE).pc
	install -m 0644 include/sys/capability.h $(FAKEROOT)$(INCDIR)/sys
	install -m 0644 $(LIBTITLE).pc $(FAKEROOT)$(PKGCONFIGDIR)/$(LIBTITLE).pc

include/sys/psx_syscall.h: ../psx/psx_syscall.h
	rm -f $@
	ln -s ../../../psx/psx_syscall.h $@

install-common-psx: install-common $(PSXTITLE).pc include/sys/psx_syscall.h
	install -m 0644 include/sys/psx_syscall.h $(FAKEROOT)$(INCDIR)/sys
	install -m 0644 $(PSXTITLE).pc $(FAKEROOT)$(PKGCONFIGDIR)/$(PSXTITLE).pc

install-common:
	mkdir -p -m 0755 $(FAKEROOT)$(INCDIR)/sys
	mkdir -p -m 0755 $(FAKEROOT)$(PKGCONFIGDIR)
	mkdir -p -m 0755 $(FAKEROOT)$(LIBDIR)

clean:
	$(LOCALCLEAN)
	rm -f $(CAPOBJS) $(CAPLIBNAME)* $(STACAPLIBNAME) $(LIBTITLE).pc
	rm -f $(PSXOBJS) $(PSXLIBNAME)* $(STAPSXLIBNAME) $(PSXTITLE).pc
	rm -f cap_names.h cap_names.list.h _makenames $(GPERF_OUTPUT) cap_test
	rm -f include/sys/psx_syscall.h
	rm -f $(CAPMAGICOBJ) $(PSXMAGICOBJ) empty loader.txt
	cd include/sys && $(LOCALCLEAN)
