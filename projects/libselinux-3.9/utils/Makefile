# Installation directories.
PREFIX ?= /usr
SBINDIR ?= $(PREFIX)/sbin

OS ?= $(shell uname)

ifeq ($(shell $(CC) -v 2>&1 | grep "clang"),)
COMPILER ?= gcc
else
COMPILER ?= clang
endif

ifeq ($(COMPILER), gcc)
EXTRA_CFLAGS = -fipa-pure-const -Wpacked-bitfield-compat -Wsync-nand -Wcoverage-mismatch \
	-Wcpp -Wformat-contains-nul -Wnormalized=nfc -Wsuggest-attribute=const \
	-Wsuggest-attribute=noreturn -Wsuggest-attribute=pure -Wtrampolines -Wjump-misses-init \
	-Wno-suggest-attribute=pure -Wno-suggest-attribute=const
endif

MAX_STACK_SIZE=8192
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
          -Wno-missing-field-initializers -Wno-sign-compare \
          -Wno-format-nonliteral -Wframe-larger-than=$(MAX_STACK_SIZE) -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 \
          -fstack-protector-all --param=ssp-buffer-size=4 -fexceptions \
          -fasynchronous-unwind-tables -fdiagnostics-show-option \
          -Werror -Wno-aggregate-return -Wno-redundant-decls -Wstrict-overflow=5 \
          $(EXTRA_CFLAGS)

override CFLAGS += $(LFS_CFLAGS)

ifeq ($(OS), Darwin)
override CFLAGS += -I/opt/local/include -I../../libsepol/include
override LDFLAGS += -L../../libsepol/src -undefined dynamic_lookup
endif

override CFLAGS += -I../include -D_GNU_SOURCE $(DISABLE_FLAGS) $(PCRE_CFLAGS)
override LDFLAGS += -L../src
override LDLIBS += -lselinux $(FTS_LDLIBS)

ifeq ($(ANDROID_HOST),y)
TARGETS=sefcontext_compile
else
TARGETS=$(patsubst %.c,%,$(sort $(wildcard *.c)))
endif

sefcontext_compile: LDLIBS += ../src/libselinux.a -lsepol

PCRE_USERS = matchpathcon sefcontext_compile selabel_compare \
	selabel_digest selabel_get_digests_all_partial_matches \
	selabel_lookup selabel_lookup_best_match \
	selabel_partial_match
$(PCRE_USERS): LDLIBS += $(PCRE_LDLIBS)

all: $(TARGETS)

install: all
	-mkdir -p $(DESTDIR)$(SBINDIR)
	install -m 755 $(TARGETS) $(DESTDIR)$(SBINDIR)

clean:
	rm -f $(TARGETS) *.o *~

distclean: clean

indent:
	../../scripts/Lindent $(wildcard *.[ch])

relabel:

