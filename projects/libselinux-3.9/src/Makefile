# Support building the Python bindings multiple times, against various Python
# runtimes (e.g. Python 2 vs Python 3) by optionally prefixing the build
# targets with "PYPREFIX":
PYTHON ?= python3
PYPREFIX ?= $(shell $(PYTHON) -c 'import sys;print("python-%d.%d" % sys.version_info[:2])')
RUBY ?= ruby
RUBYPREFIX ?= $(notdir $(RUBY))
PKG_CONFIG ?= pkg-config

# Installation directories.
PREFIX ?= /usr
LIBDIR ?= $(PREFIX)/lib
SHLIBDIR ?= /lib
INCLUDEDIR ?= $(PREFIX)/include
PYINC ?= $(shell $(PKG_CONFIG) --cflags $(PYPREFIX))
PYLIBS ?= $(shell $(PKG_CONFIG) --libs $(PYPREFIX))
PYTHONLIBDIR ?= $(shell $(PYTHON) -c "import sysconfig; print(sysconfig.get_path('platlib', vars={'platbase': '$(PREFIX)', 'base': '$(PREFIX)'}))")
PYCEXT ?= $(shell $(PYTHON) -c 'import importlib.machinery;print(importlib.machinery.EXTENSION_SUFFIXES[0])')
RUBYINC ?= $(shell $(RUBY) -e 'puts "-I" + RbConfig::CONFIG["rubyarchhdrdir"] + " -I" + RbConfig::CONFIG["rubyhdrdir"]')
RUBYLIBS ?= $(shell $(RUBY) -e 'puts "-L" + RbConfig::CONFIG["libdir"] + " -L" + RbConfig::CONFIG["archlibdir"] + " " + RbConfig::CONFIG["LIBRUBYARG_SHARED"]')
RUBYINSTALL ?= $(shell $(RUBY) -e 'puts RbConfig::CONFIG["vendorarchdir"]')

VERSION = $(shell cat ../VERSION)
LIBVERSION = 1

OS ?= $(shell uname)

ifeq ($(shell $(CC) -v 2>&1 | grep "clang"),)
COMPILER ?= gcc
else
COMPILER ?= clang
endif

LIBA=libselinux.a 
TARGET=libselinux.so
LIBPC=libselinux.pc
SWIGIF= selinuxswig_python.i selinuxswig_python_exception.i
SWIGRUBYIF= selinuxswig_ruby.i
SWIGCOUT= selinuxswig_python_wrap.c
SWIGPYOUT= selinux.py
SWIGRUBYCOUT= selinuxswig_ruby_wrap.c
SWIGLOBJ:= $(patsubst %.c,$(PYPREFIX)%.lo,$(SWIGCOUT))
SWIGRUBYLOBJ:= $(patsubst %.c,%.lo,$(SWIGRUBYCOUT)) 
SWIGSO=$(PYPREFIX)_selinux.so
SWIGFILES=$(SWIGSO) $(SWIGPYOUT)
SWIGRUBYSO=$(RUBYPREFIX)_selinux.so
LIBSO=$(TARGET).$(LIBVERSION)
AUDIT2WHYLOBJ=$(PYPREFIX)audit2why.lo
AUDIT2WHYSO=$(PYPREFIX)audit2why.so

# If no specific libsepol.a is specified, fall back on LDFLAGS search path
# Otherwise, as $(LIBSEPOLA) already appears in the dependencies, there
# is no need to define a value for LDLIBS_LIBSEPOLA
ifeq ($(LIBSEPOLA),)
        LDLIBS_LIBSEPOLA := -l:libsepol.a
endif

GENERATED=$(SWIGCOUT) $(SWIGRUBYCOUT) $(SWIGCOUT) selinuxswig_python_exception.i
SRCS= $(filter-out $(GENERATED) audit2why.c, $(sort $(wildcard *.c)))

MAX_STACK_SIZE=32768

ifeq ($(COMPILER), gcc)
EXTRA_CFLAGS = -Wlogical-op -Wpacked-bitfield-compat -Wsync-nand \
	-Wcoverage-mismatch -Wcpp -Wformat-contains-nul -Wnormalized=nfc -Wsuggest-attribute=const \
	-Wsuggest-attribute=noreturn -Wsuggest-attribute=pure -Wtrampolines -Wjump-misses-init \
	-Wno-suggest-attribute=pure -Wno-suggest-attribute=const -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 \
	-Wstrict-overflow=5 -fno-semantic-interposition
else
EXTRA_CFLAGS = -Wunused-command-line-argument
endif

OBJS= $(patsubst %.c,%.o,$(SRCS))
LOBJS= $(patsubst %.c,%.lo,$(SRCS))
CFLAGS ?= -O -Wall -W -Wundef -Wformat-y2k -Wformat-security -Winit-self -Wmissing-include-dirs \
          -Wunused -Wunknown-pragmas -Wstrict-aliasing -Wshadow -Wpointer-arith \
          -Wbad-function-cast -Wcast-align -Wwrite-strings -Waggregate-return \
          -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes \
          -Wmissing-declarations -Wmissing-noreturn -Wmissing-format-attribute \
          -Wredundant-decls -Wnested-externs -Winline -Winvalid-pch -Wvolatile-register-var \
          -Wdisabled-optimization -Wbuiltin-macro-redefined \
          -Wattributes -Wmultichar \
          -Wdeprecated-declarations -Wdiv-by-zero -Wdouble-promotion -Wendif-labels -Wextra \
          -Wformat-extra-args -Wformat-zero-length -Wformat=2 -Wmultichar \
          -Woverflow -Wpointer-to-int-cast -Wpragmas \
          -Wframe-larger-than=$(MAX_STACK_SIZE) \
          -fstack-protector-all --param=ssp-buffer-size=4 -fexceptions \
          -fasynchronous-unwind-tables -fdiagnostics-show-option \
          -Werror -Wno-aggregate-return \
          $(EXTRA_CFLAGS)

override CFLAGS += $(LFS_CFLAGS)

LD_SONAME_FLAGS=-soname,$(LIBSO),--version-script=libselinux.map,-z,defs,-z,relro

ifeq ($(OS), Darwin)
override CFLAGS += -I/opt/local/include
override LDFLAGS += -L/opt/local/lib -undefined dynamic_lookup
LD_SONAME_FLAGS=-install_name,$(LIBSO)
endif

# override with -lfts when building on Musl libc to use fts-standalone
FTS_LDLIBS ?=

override CFLAGS += -I../include -D_GNU_SOURCE $(DISABLE_FLAGS) $(PCRE_CFLAGS)

# check for strlcpy(3) availability
H := \#
ifeq (yes,$(shell printf '${H}include <string.h>\nint main(void){char d[2];const char *s="a";return (size_t)strlcpy(d,s,sizeof(d))>=sizeof(d);}' | $(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -Werror=implicit-function-declaration -x c -o /dev/null - >/dev/null 2>&1 && echo yes))
override CFLAGS += -DHAVE_STRLCPY
endif

# check for reallocarray(3) availability
H := \#
ifeq (yes,$(shell printf '${H}include <stdlib.h>\nint main(void){return reallocarray(NULL,0,0)==NULL;}' | $(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -Werror=implicit-function-declaration -x c -o /dev/null - >/dev/null 2>&1 && echo yes))
override CFLAGS += -DHAVE_REALLOCARRAY
endif

SWIG_CFLAGS += -Wno-error -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-parameter \
		-Wno-shadow -Wno-uninitialized -Wno-missing-prototypes -Wno-missing-declarations \
		-Wno-deprecated-declarations

RANLIB ?= ranlib

ARCH := $(patsubst i%86,i386,$(shell uname -m))
ifneq (,$(filter i386,$(ARCH)))
TLSFLAGS += -mno-tls-direct-seg-refs
endif

ifeq ($(ANDROID_HOST),y)
DISABLE_FLAGS+= -DNO_MEDIA_BACKEND -DNO_DB_BACKEND -DNO_X_BACKEND \
	-DBUILD_HOST
SRCS= callbacks.c freecon.c label.c label_file.c \
	label_backends_android.c regex.c label_support.c \
	matchpathcon.c setrans_client.c sha1.c booleans.c
LABEL_BACKEND_ANDROID=y
endif

ifneq ($(LABEL_BACKEND_ANDROID),y)
SRCS:= $(filter-out label_backends_android.c, $(SRCS))
DISABLE_FLAGS+= -DNO_ANDROID_BACKEND
endif

ifeq ($(DISABLE_X11),y)
SRCS:= $(filter-out label_x.c, $(SRCS))
endif

SWIGRUBY = swig -Wall -ruby -o $(SWIGRUBYCOUT) -outdir ./ $(DISABLE_FLAGS)

all: $(LIBA) $(LIBPC)
ifneq ($(DISABLE_SHARED),y)
all: $(LIBSO)
endif

pywrap: all selinuxswig_python_exception.i
	CFLAGS="$(CPPFLAGS) $(CFLAGS) $(SWIG_CFLAGS)" $(PYTHON) setup.py build_ext

rubywrap: all $(SWIGRUBYSO)

$(SWIGRUBYLOBJ): $(SWIGRUBYCOUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SWIG_CFLAGS) $(RUBYINC) -fPIC -DSHARED -c -o $@ $<

$(SWIGRUBYSO): $(SWIGRUBYLOBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -L. -shared -o $@ $^ -lselinux $(RUBYLIBS)

$(LIBA): $(OBJS)
	$(AR) rcs $@ $^
	$(RANLIB) $@

$(LIBSO): $(LOBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -shared -o $@ $^ $(PCRE_LDLIBS) $(FTS_LDLIBS) -ldl -Wl,$(LD_SONAME_FLAGS)
	ln -sf $@ $(TARGET)

$(LIBPC): $(LIBPC).in ../VERSION
	sed -e 's/@VERSION@/$(VERSION)/; s:@prefix@:$(PREFIX):; s:@libdir@:$(LIBDIR):; s:@includedir@:$(INCLUDEDIR):; s:@PCRE_MODULE@:$(PCRE_MODULE):' < $< > $@

selinuxswig_python_exception.i: exception.sh ../include/selinux/selinux.h
	bash -e exception.sh > $@ || (rm -f $@ ; false)

%.o:  %.c policy.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TLSFLAGS) -c -o $@ $<

%.lo:  %.c policy.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -DSHARED -c -o $@ $<

$(SWIGRUBYCOUT): $(SWIGRUBYIF)
	$(SWIGRUBY) $<

install: all 
	test -d $(DESTDIR)$(LIBDIR) || install -m 755 -d $(DESTDIR)$(LIBDIR)
	install -m 644 $(LIBA) $(DESTDIR)$(LIBDIR)
	test -d $(DESTDIR)$(LIBDIR)/pkgconfig || install -m 755 -d $(DESTDIR)$(LIBDIR)/pkgconfig
	install -m 644 $(LIBPC) $(DESTDIR)$(LIBDIR)/pkgconfig
ifneq ($(DISABLE_SHARED),y)
	test -d $(DESTDIR)$(SHLIBDIR) || install -m 755 -d $(DESTDIR)$(SHLIBDIR)
	install -m 755 $(LIBSO) $(DESTDIR)$(SHLIBDIR)
	ln -sf --relative $(DESTDIR)$(SHLIBDIR)/$(LIBSO) $(DESTDIR)$(LIBDIR)/$(TARGET)
endif

install-pywrap: pywrap
	CFLAGS="$(CPPFLAGS) $(CFLAGS) $(SWIG_CFLAGS)" $(PYTHON) -m pip install --prefix=$(PREFIX) `test -n "$(DESTDIR)" && echo --root $(DESTDIR) --ignore-installed --no-deps` $(PYTHON_SETUP_ARGS) .
	install -m 644 $(SWIGPYOUT) $(DESTDIR)$(PYTHONLIBDIR)/selinux/__init__.py
	ln -sf --relative $(DESTDIR)$(PYTHONLIBDIR)/selinux/_selinux$(PYCEXT) $(DESTDIR)$(PYTHONLIBDIR)/_selinux$(PYCEXT)

install-rubywrap: rubywrap
	test -d $(DESTDIR)$(RUBYINSTALL) || install -m 755 -d $(DESTDIR)$(RUBYINSTALL) 
	install -m 755 $(SWIGRUBYSO) $(DESTDIR)$(RUBYINSTALL)/selinux.so

relabel:
	/sbin/restorecon $(DESTDIR)$(SHLIBDIR)/$(LIBSO)

clean-pywrap:
	-rm -f $(SWIGLOBJ) $(SWIGSO) $(AUDIT2WHYLOBJ) $(AUDIT2WHYSO)
	$(PYTHON) setup.py clean
	-rm -rf build *~ \#* *pyc .#* selinux.egg-info/

clean-rubywrap:
	-rm -f $(SWIGRUBYLOBJ) $(SWIGRUBYSO)

clean: clean-pywrap clean-rubywrap
	-rm -f $(LIBPC) $(OBJS) $(LOBJS) $(LIBA) $(LIBSO) $(TARGET) *.o *.lo *~

distclean: clean
	rm -f $(GENERATED) $(SWIGFILES)

indent:
	../../scripts/Lindent $(filter-out $(GENERATED),$(wildcard *.[ch]))

.PHONY: all clean clean-pywrap clean-rubywrap pywrap rubywrap swigify install install-pywrap install-rubywrap distclean
