
CYGWINFLAGS = $(LTLIBINTL)
if CYGWIN
usrbin_exec_PROGRAMS =
endif

AM_CPPFLAGS = \
	-include $(top_builddir)/config.h \
	-I$(top_srcdir)/local \
	-I$(top_srcdir)/library/include \
	-Wno-unused-result \
	-DLOCALEDIR=\"$(localedir)\" \
	@HARDEN_CFLAGS@

AM_LDFLAGS = @HARDEN_LDFLAGS@

PACKAGE_VERSION = @PACKAGE_VERSION@

ACLOCAL_AMFLAGS = -I m4
SUBDIRS = \
	local \
	po-man \
	po \
	testsuite

AM_CFLAGS = -Ilibrary/include
LDADD = ./library/libproc2.la $(CYGWINFLAGS)

if CYGWIN
AM_LDFLAGS += ./library/libproc2.la $(CYGWINFLAGS)
endif

transform =

if !CYGWIN
transform += s/pscommand/ps/; $(program_transform_name)
sbin_PROGRAMS = \
	src/sysctl
else
transform += s/pscommand/procps/; $(program_transform_name)
endif

if !CYGWIN
bin_PROGRAMS = \
	src/ps/pscommand \
	src/free \
	src/pgrep \
	src/pkill \
	src/pmap \
	src/pwdx \
	src/tload \
	src/uptime \
	src/vmstat
if BUILD_PIDWAIT
bin_PROGRAMS += src/pidwait
endif
else
usrbin_exec_PROGRAMS += \
	src/ps/pscommand \
	src/free \
	src/pgrep \
	src/pkill \
	src/pmap \
	src/uptime \
	src/vmstat
endif

lib_LTLIBRARIES = \
	library/libproc2.la

dist_man_MANS = \
	man/free.1 \
	man/pgrep.1 \
	man/pkill.1 \
	man/pmap.1 \
	man/uptime.1 \
	man/vmstat.8 \
	man/procps.3 \
	man/procps_pids.3 \
	man/procps_misc.3

if !CYGWIN
dist_man_MANS += \
	man/pwdx.1 \
	man/tload.1 \
	man/sysctl.8 \
	man/sysctl.conf.5 \
	man/ps.1

if BUILD_PIDWAIT
dist_man_MANS += man/pidwait.1
endif
endif

EXTRA_DIST = \
	.version \
	autogen.sh \
	COPYING.LIB \
	local/git-version-gen \
	doc/CodingStyle.md \
	doc/TODO \
	src/ps/HACKING \
	src/ps/regression \
	$(DIST_MAN_POTS)

if !CYGWIN
EXTRA_DIST += \
	sysctl.conf
endif

procpsngdir = $(docdir)
dist_procpsng_DATA = \
	doc/bugs.md \
	doc/FAQ \
	doc/libproc.supp

if BUILD_PIDOF
if !CYGWIN
bin_PROGRAMS += src/pidof
else
usrbin_exec_PROGRAMS += src/pidof
endif
dist_man_MANS += man/pidof.1
src_pidof_SOURCES = src/pidof.c local/fileutils.c
else
  EXTRA_DIST += man/pidof.1
endif

if BUILD_KILL
if CYGWIN
transform += ;s/^kill/prockill/; $(program_transform_name)
usrbin_exec_PROGRAMS += src/kill
else
bin_PROGRAMS += src/kill
endif
dist_man_MANS += man/kill.1
src_kill_SOURCES = src/kill.c local/strutils.c local/fileutils.c local/signals.c
src_kill_LDADD = $(LTLIBINTL)
else
  EXTRA_DIST += man/kill.1
endif

if BUILD_W
if CYGWIN
usrbin_exec_PROGRAMS += src/w
else
bin_PROGRAMS += src/w
endif

dist_man_MANS += man/w.1
src_w_SOURCES = src/w.c local/fileutils.c
else
  EXTRA_DIST += man/w.1
endif

if WITH_NCURSES
if !CYGWIN
bin_PROGRAMS += \
	src/watch \
	src/top/top
else
usrbin_exec_PROGRAMS += \
	src/watch \
	src/top/top
endif

dist_man_MANS += \
	man/watch.1 \
	man/top.1
if !CYGWIN
bin_PROGRAMS += \
	src/slabtop
dist_man_MANS += \
	man/slabtop.1
src_slabtop_SOURCES = src/slabtop.c local/strutils.c local/fileutils.c
src_slabtop_CFLAGS = @NCURSES_CFLAGS@
src_slabtop_LDADD = $(LDADD) @NCURSES_LIBS@
endif
src_watch_SOURCES = src/watch.c local/strutils.c local/fileutils.c
src_watch_CFLAGS = @NCURSES_CFLAGS@
src_watch_LDADD = @NCURSES_LIBS@ $(CYGWINFLAGS)
src_top_top_SOURCES = \
	src/top/top.h \
	src/top/top.c \
	src/top/top_nls.h \
	src/top/top_nls.c \
	local/fileutils.c \
	local/signals.c
if CYGWIN
src_top_top_SOURCES += local/strverscmp.c
endif

src_top_top_CFLAGS = @NCURSES_CFLAGS@
src_top_top_LDADD = $(LDADD) @NCURSES_LIBS@ $(DL_LIB)
endif

if BUILD_SKILL
if !CYGWIN
bin_PROGRAMS += \
	src/skill \
	src/snice
else
usrbin_exec_PROGRAMS += \
	src/skill \
	src/snice
endif
src_skill_SOURCES = src/skill.c local/strutils.c local/fileutils.c local/signals.c
src_snice_SOURCES = src/skill.c local/strutils.c local/fileutils.c local/signals.c
dist_man_MANS += \
	man/skill.1 \
	man/snice.1
else
  EXTRA_DIST += \
	man/skill.1 \
	man/snice.1
endif

src_free_SOURCES = src/free.c local/strutils.c local/fileutils.c
src_pgrep_SOURCES = src/pgrep.c local/fileutils.c local/signals.c
src_pkill_SOURCES = src/pgrep.c local/fileutils.c local/signals.c
src_pmap_SOURCES = src/pmap.c local/fileutils.c
if BUILD_PIDWAIT
src_pidwait_SOURCES = src/pgrep.c local/fileutils.c local/signals.c
endif
if !CYGWIN
src_pwdx_SOURCES = src/pwdx.c local/fileutils.c
src_pwdx_LDADD= $(CYGWINFLAGS)
src_sysctl_SOURCES = \
	src/sysctl.c \
	local/fileutils.c \
	local/procio.c
src_sysctl_LDADD= $(LTLIBINTL)
endif
src_tload_SOURCES = src/tload.c local/strutils.c local/fileutils.c
src_uptime_SOURCES = src/uptime.c local/fileutils.c
src_vmstat_SOURCES = src/vmstat.c local/strutils.c local/fileutils.c


# See http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
LIBproc2_CURRENT=0
LIBproc2_REVISION=2
LIBproc2_AGE=0

library_libproc2_la_LIBADD = $(LIB_KPARTS)

if WITH_SYSTEMD
library_libproc2_la_LIBADD += @SYSTEMD_LIBS@
endif

if WITH_ELOGIND
library_libproc2_la_LIBADD += @ELOGIND_LIBS@
endif

library_libproc2_la_LDFLAGS = \
	-version-info $(LIBproc2_CURRENT):$(LIBproc2_REVISION):$(LIBproc2_AGE) \
	-no-undefined \
	-Wl,--version-script=$(top_srcdir)/library/libproc2.sym

library_libproc2_la_SOURCES = \
	library/devname.c \
	library/include/devname.h \
	library/diskstats.c \
	library/include/diskstats.h \
	library/escape.c \
	library/include/escape.h \
	library/include/procps-private.h \
	library/meminfo.c \
	library/include/meminfo.h \
	library/include/misc.h \
	library/namespace.c \
	library/numa.c \
	library/include/numa.h \
	library/pids.c \
	library/include/pids.h \
	library/pwcache.c \
	library/include/pwcache.h \
	library/readproc.c \
	library/include/readproc.h \
	library/slabinfo.c \
	library/include/slabinfo.h \
	library/stat.c \
	library/include/stat.h \
	library/sysinfo.c \
	library/version.c \
	library/vmstat.c \
	library/include/vmstat.h \
	library/wchan.c \
	library/include/wchan.h \
	library/uptime.c \
	library/include/xtra-procps-debug.h

library_libproc2_la_includedir = $(includedir)/libproc2/
library_libproc2_la_include_HEADERS = \
	library/include/diskstats.h \
	library/include/meminfo.h \
	library/include/misc.h \
	library/include/pids.h \
	library/include/slabinfo.h \
	library/include/stat.h \
	library/include/vmstat.h \
	library/include/xtra-procps-debug.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = \
	library/libproc2.pc

EXTRA_DIST += library/libproc2.sym

# ps/pscommand

src_ps_pscommand_SOURCES =  \
	src/ps/common.h \
	src/ps/display.c \
	src/ps/global.c \
	src/ps/help.c \
	src/ps/output.c \
	src/ps/parser.c \
	src/ps/select.c \
	src/ps/signames.c \
	src/ps/sortformat.c \
	src/ps/stacktrace.c \
	local/fileutils.c \
	local/signals.c


# Test programs required for dejagnu or standalone testing
check_PROGRAMS = \
	src/tests/test_strutils \
	src/tests/test_fileutils \
	src/tests/test_process \
	src/tests/test_strtod_nol \
	src/tests/test_shm

src_tests_test_strutils_SOURCES = src/tests/test_strutils.c local/strutils.c
src_tests_test_strutils_LDADD = $(CYGWINFLAGS)
src_tests_test_fileutils_SOURCES = src/tests/test_fileutils.c local/fileutils.c
src_tests_test_fileutils_LDADD = $(CYGWINFLAGS)
src_tests_test_process_SOURCES = src/tests/test_process.c
src_tests_test_process_LDADD = $(CYGWINFLAGS)
src_tests_test_strtod_nol_SOURCES = src/tests/test_strtod_nol.c local/strutils.c
src_tests_test_strtod_nol_LDADD = $(CYGWINFLAGS)
src_tests_test_shm_SOURCES = src/tests/test_shm.c local/strutils.c
src_tests_test_shm_LDADD = $(CYGWINFLAGS)

check_PROGRAMS += \
	library/tests/test_Itemtables \
	library/tests/test_pids \
	library/tests/test_uptime \
	library/tests/test_sysinfo \
	library/tests/test_version \
	library/tests/test_namespace

library_tests_test_Itemtables_SOURCES = library/tests/test_Itemtables.c
library_tests_test_Itemtables_LDADD = library/libproc2.la
library_tests_test_pids_SOURCES = library/tests/test_pids.c
library_tests_test_pids_LDADD = library/libproc2.la
library_tests_test_uptime_SOURCES = library/tests/test_uptime.c
library_tests_test_uptime_LDADD = library/libproc2.la
library_tests_test_sysinfo_SOURCES = library/tests/test_sysinfo.c
library_tests_test_sysinfo_LDADD = library/libproc2.la
library_tests_test_version_SOURCES = library/tests/test_version.c
library_tests_test_version_LDADD = library/libproc2.la
library_tests_test_namespace_SOURCES = library/tests/test_namespace.c
library_tests_test_namespace_LDADD = library/libproc2.la

if CYGWIN
	src_skill_LDADD = $(CYGWINFLAGS)
	src_kill_LDADD = $(CYGWINFLAGS)
	src_free_LDADD = $(CYGWINFLAGS)
	src_pgrep_LDADD = $(CYGWINFLAGS)
	src_pkill_LDADD = $(CYGWINFLAGS)
	src_pidof_LDADD = $(CYGWINFLAGS)
	src_pmap_LDADD = $(CYGWINFLAGS)
	src_snice_LDADD = $(CYGWINFLAGS)
	src_tload_LDADD = $(CYGWINFLAGS)
	src_uptime_LDADD = $(CYGWINFLAGS)
	src_w_LDADD = $(CYGWINFLAGS)
	src_vmstat_LDADD = $(CYGWINFLAGS)
endif

if EXAMPLE_FILES
if !CYGWIN
sysconf_DATA = sysctl.conf
endif
endif

BUILT_SOURCES = $(top_srcdir)/.version

check-lib: clean
	$(MAKE) CFLAGS=-DITEMTABLE_DEBUG library/tests/test_Itemtables
	$(top_builddir)/library/tests/test_Itemtables
	$(MAKE) clean &>/dev/null

# Test programs not used by dejagnu but run directly
TESTS = \
	library/tests/test_pids \
	library/tests/test_uptime \
	library/tests/test_sysinfo \
	library/tests/test_version \
	library/tests/test_namespace \
	src/tests/test_fileutils \
	src/tests/test_strtod_nol

# Automake should do this, but it doesn't
check: $(check_PROGRAMS) $(PROGRAMS)

$(top_srcdir)/.version:
	touch $(top_srcdir)/.version
	echo $(VERSION) > $@-t && mv $@-t $@
dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version

get-trans:
	echo "Getting the latest translations from translationproject.org..."
	rsync -Lrtvz  translationproject.org::tp/latest/procps-ng/  po
	rsync -Lrtvz  translationproject.org::tp/latest/procps-ng-man/  po-man
	rm -f po-man/man.stamp
	make -C po-man man.stamp
