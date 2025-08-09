ACLOCAL_AMFLAGS = -I build-aux

AM_CFLAGS = -I$(top_srcdir)/include

EXTRA_DIST = LICENSE 3rdparty/libsais-LICENSE PORTING.md README.md build-aux/git-version-gen CMakeLists.txt

pkgconfig_DATA = bzip3.pc

include_HEADERS = include/libbz3.h
noinst_HEADERS = include/common.h \
						 include/libsais.h \
						 include/yarg.h

lib_LTLIBRARIES = libbzip3.la
libbzip3_la_SOURCES = src/libbz3.c
libbzip3_la_LDFLAGS = -no-undefined -version-info 1:0:0

bin_PROGRAMS = bzip3
bzip3_CFLAGS = $(AM_CFLAGS)
bzip3_SOURCES = src/main.c
if ENABLE_STATIC
bzip3_SOURCES += $(libbzip3_la_SOURCES)
else
bzip3_LDADD = libbzip3.la
endif

dist_man_MANS = bzip3.1 bz3cat.1 bz3more.1 bz3less.1 bz3most.1 bz3grep.1 bunzip3.1

dist_bin_SCRIPTS = bz3cat bz3more bz3less bz3most bz3grep bunzip3

CLEANFILES = $(bin_PROGRAMS)

# End standard generic autotools stuff

# Begin special handling for autoconf VERSION being updated on commit

BUILT_SOURCES = .version
CLEANFILES += $(BUILT_SOURCES) .version-prev

src/bzip3-main.$(OBJEXT): .version

_BRANCH_REF != $(AWK) '{print ".git/" $$2}' .git/HEAD 2>/dev/null ||:

.version: $(_BRANCH_REF)
	@if [ -e "$(srcdir)/.tarball-version" ]; then \
		printf "$(VERSION)" > $@; \
	else \
		touch "$@-prev"; \
		if [ -e "$@" ]; then \
			cp "$@" "$@-prev"; \
		fi; \
		./build-aux/git-version-gen "$(srcdir)/.tarball-version" > $@; \
		cmp -s "$@" "$@-prev" || autoreconf configure.ac --force; \
	fi

dist-hook:
	printf "$(VERSION)" > "$(distdir)/.tarball-version"

# Begin developer convenience targets

.PHONY: format
format: $(bzip3_SOURCES) $(libbzip3_la_SOURCES) $(include_HEADERS) $(noinst_HEADERS)
	clang-format -i $^ examples/*.c

.PHONY: cloc
cloc: $(bzip3_SOURCES) $(libbzip3_la_SOURCES) $(include_HEADERS) $(noinst_HEADERS)
	cloc $^

CLEANFILES += LICENSE2
.PHONY: roundtrip

BZIP3 := bzip3$(EXEEXT)

roundtrip: $(BZIP3)
	rm -f $(builddir)/LICENSE2
	./$(BZIP3) -v -feb 6 $(srcdir)/LICENSE $(builddir)/LICENSE.bz3
	./$(BZIP3) -v -d $(builddir)/LICENSE.bz3 $(builddir)/LICENSE2
	cmp $(srcdir)/LICENSE $(builddir)/LICENSE2
	-command -v md5sum >/dev/null 2>&1 && md5sum $(builddir)/LICENSE.bz3

.PHONY: test
test: $(BZIP3)
	./$(BZIP3) -d < $(srcdir)/examples/shakespeare.txt.bz3 | cmp - $(srcdir)/examples/shakespeare.txt
