AM_CPPFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir)

if HAVE_CMOCKA
TESTS = $(check_PROGRAMS)

check_PROGRAMS = \
    test_adds \
    test_atoi_strtoi \
    test_chkname \
    test_sprintf \
    test_strncpy \
    test_strtcpy \
    test_xasprintf \
    test_zustr2stp

if ENABLE_LOGIND
check_PROGRAMS += \
    test_logind
endif # ENABLE_LOGIND

check_PROGRAMS += \
    $(NULL)

test_adds_SOURCES = \
    ../../lib/adds.c \
    test_adds.c \
    $(NULL)
test_adds_CFLAGS = \
    $(AM_FLAGS) \
    $(NULL)
test_adds_LDFLAGS = \
    $(NULL)
test_adds_LDADD = \
    $(CMOCKA_LIBS) \
    $(NULL)

test_atoi_strtoi_SOURCES = \
    ../../lib/atoi/strtoi.c \
    test_atoi_strtoi.c \
    $(NULL)
test_atoi_strtoi_CFLAGS = \
    $(AM_CFLAGS) \
    $(NULL)
test_atoi_strtoi_LDFLAGS = \
    $(NULL)
test_atoi_strtoi_LDADD = \
    $(CMOCKA_LIBS) \
    $(NULL)

test_chkname_SOURCES = \
    ../../lib/chkname.c \
    test_chkname.c \
    $(NULL)
test_chkname_CFLAGS = \
    $(AM_CFLAGS) \
    $(NULL)
test_chkname_LDFLAGS = \
    $(NULL)
test_chkname_LDADD = \
    $(CMOCKA_LIBS) \
    $(NULL)

test_logind_SOURCES = \
    ../../lib/logind.c \
    test_logind.c \
    $(NULL)
test_logind_CFLAGS = \
    $(AM_CFLAGS) \
    -lsystemd \
    $(NULL)
test_logind_LDFLAGS = \
    -Wl,-wrap,prefix_getpwnam \
    -Wl,-wrap,sd_uid_get_sessions \
    $(NULL)
test_logind_LDADD = \
    $(CMOCKA_LIBS) \
    $(LIBSYSTEMD) \
    $(NULL)

test_sprintf_SOURCES = \
    ../../lib/string/sprintf.c \
    test_sprintf.c \
    $(NULL)
test_sprintf_CFLAGS = \
    $(AM_CFLAGS) \
    $(NULL)
test_sprintf_LDFLAGS = \
    $(NULL)
test_sprintf_LDADD = \
    $(CMOCKA_LIBS) \
    $(NULL)

test_strncpy_SOURCES = \
    test_strncpy.c \
    $(NULL)
test_strncpy_CFLAGS = \
    $(AM_CFLAGS) \
    $(NULL)
test_strncpy_LDFLAGS = \
    $(NULL)
test_strncpy_LDADD = \
    $(CMOCKA_LIBS) \
    $(NULL)

test_strtcpy_SOURCES = \
    ../../lib/string/strtcpy.c \
    test_strtcpy.c \
    $(NULL)
test_strtcpy_CFLAGS = \
    $(AM_CFLAGS) \
    $(NULL)
test_strtcpy_LDFLAGS = \
    $(NULL)
test_strtcpy_LDADD = \
    $(CMOCKA_LIBS) \
    $(NULL)

test_xasprintf_SOURCES = \
    ../../lib/string/sprintf.c \
    test_xasprintf.c \
    $(NULL)
test_xasprintf_CFLAGS = \
    $(AM_CFLAGS) \
    $(NULL)
test_xasprintf_LDFLAGS = \
    -Wl,-wrap,vasprintf \
    -Wl,-wrap,exit \
    $(NULL)
test_xasprintf_LDADD = \
    $(CMOCKA_LIBS) \
    $(NULL)

test_zustr2stp_SOURCES = \
    test_zustr2stp.c \
    $(NULL)
test_zustr2stp_CFLAGS = \
    $(AM_CFLAGS) \
    $(NULL)
test_zustr2stp_LDFLAGS = \
    $(NULL)
test_zustr2stp_LDADD = \
    $(CMOCKA_LIBS) \
    $(NULL)

endif # HAVE_CMOCKA
