NULL =

SUBDIRS = \
	helpers    \
	libkbdfile \
	libkeymap  \
	$(NULL)

AUTOM4TE = $(SHELL) $(top_srcdir)/config/missing --run autom4te
AUTOTEST = $(AUTOM4TE) --language=autotest

CHECK_KEYWORDS ?= unittest

ATCONFIG  = $(abs_builddir)/atconfig
ATPACKAGE = $(abs_builddir)/atpackage
TESTSUITE = $(abs_builddir)/testsuite

EXTRA_DIST = \
	data                   \
	e2e-clrunimap.at       \
	e2e-dumpkeys.at        \
	e2e-getunimap.at       \
	e2e-kbdinfo.at         \
	e2e-kbd_mode.at        \
	e2e-loadkeys.at        \
	e2e-loadunimap.at      \
	e2e-psfxtable.at       \
	e2e-setfont.at         \
	e2e-setvtrgb.at        \
	e2e.at                 \
	libkbdfile.at          \
	libkeymap.at           \
	testsuite.at           \
	$(NULL)

CLEANFILES = \
	$(ATCONFIG)      \
	$(ATPACKAGE)     \
	$(TESTSUITE)     \
	$(TESTSUITE).log \
	$(NULL)

all: $(TESTSUITE)

# The `:;' works around a Bash 3.2 bug when the output is not writable.
$(ATPACKAGE): $(top_srcdir)/configure.ac
	:;{ \
	    echo '# Signature of the current package.';                       \
	    echo 'm4_define([AT_PACKAGE_NAME],      [$(PACKAGE_NAME)])';      \
	    echo 'm4_define([AT_PACKAGE_TARNAME],   [$(PACKAGE_TARNAME)])';   \
	    echo 'm4_define([AT_PACKAGE_VERSION],   [$(PACKAGE_VERSION)])';   \
	    echo 'm4_define([AT_PACKAGE_STRING],    [$(PACKAGE_STRING)])';    \
	    echo 'm4_define([AT_PACKAGE_BUGREPORT], [$(PACKAGE_BUGREPORT)])'; \
	    echo 'm4_define([AT_PACKAGE_URL],       [$(PACKAGE_URL)])';       \
	} >'$(ATPACKAGE)'

$(ATCONFIG): $(top_builddir)/config.status
	cd $(top_builddir) && \
	$(SHELL) ./config.status tests/atconfig

check-local: $(ATCONFIG) $(TESTSUITE)
	$(SHELL) '$(TESTSUITE)' $(foreach kw,$(CHECK_KEYWORDS), --keywords=$(kw)) $(TESTSUITEFLAGS)

installcheck-local: $(ATCONFIG) $(TESTSUITE)
	$(SHELL) '$(TESTSUITE)' $(foreach kw,$(CHECK_KEYWORDS), --keywords=$(kw)) AUTOTEST_PATH='$(bindir)' $(TESTSUITEFLAGS)

$(TESTSUITE): $(ATPACKAGE) $(ATCONFIG)
	$(AUTOTEST) -I '$(srcdir)' -o - testsuite.at > $@
