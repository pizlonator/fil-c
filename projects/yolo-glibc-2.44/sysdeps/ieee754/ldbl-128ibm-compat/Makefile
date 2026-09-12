ifeq ($(subdir),libio)
ldbl-extra-routines += fwscanf \
		       fwprintf \
		       obprintf \
		       swscanf \
		       swprintf \
		       wscanf \
		       vasprintf \
		       vdprintf \
		       vscanf \
		       vsprintf \
		       vsnprintf \
		       vsscanf \
		       vswprintf \
		       vswscanf \
		       vwprintf \
		       vwscanf \
		       wprintf
endif

ifeq ($(subdir),wcsmbs)
ldbl-extra-routines += isoc99_fwscanf \
		       isoc99_swscanf \
		       isoc99_wscanf \
		       isoc99_vfwscanf \
		       isoc99_vswscanf \
		       isoc99_vwscanf \
		       isoc23_fwscanf \
		       isoc23_swscanf \
		       isoc23_wscanf \
		       isoc23_vfwscanf \
		       isoc23_vswscanf \
		       isoc23_vwscanf

tests-internal += test-wcstold-ibm128 test-wcstold-ieee128
CFLAGS-test-wcstold-ieee128.c += -mfloat128 -mabi=ieeelongdouble -Wno-psabi
CFLAGS-test-wcstold-ibm128.c += -mabi=ibmlongdouble -Wno-psabi

endif

ifeq ($(subdir),stdio-common)
ldbl-extra-routines += printf_size \
		       asprintf \
		       dprintf \
		       fprintf \
		       printf \
		       snprintf \
		       sprintf \
		       vfprintf \
		       vprintf \
		       vfwprintf \
		       fscanf \
		       scanf \
		       sscanf \
		       vfscanf \
		       vfwscanf \
		       isoc99_fscanf \
		       isoc99_scanf \
		       isoc99_sscanf \
		       isoc99_vfscanf \
		       isoc99_vscanf \
		       isoc99_vsscanf \
		       isoc23_fscanf \
		       isoc23_scanf \
		       isoc23_sscanf \
		       isoc23_vfscanf \
		       isoc23_vscanf \
		       isoc23_vsscanf

# Printing long double values with IEEE binary128 format reuses part
# of the internal float128 implementation (__printf_fp, __printf_fphex,
# and __float128 variables and union members).  Likewise, reading these
# values reuses __strtof128_internal.  Thus, the compilation of the
# following functions, must have -mfloat128 and -mabi=ibmlongdouble
# passed to the compiler.
CFLAGS-vfprintf-internal.c += -mfloat128 -mabi=ibmlongdouble
CFLAGS-vfwprintf-internal.c += -mfloat128 -mabi=ibmlongdouble
CFLAGS-vfscanf-internal.c += -mfloat128 -mabi=ibmlongdouble
CFLAGS-vfwscanf-internal.c += -mfloat128 -mabi=ibmlongdouble

ifeq ($(subdir),stdio-common)
# We already have the long double conversion defined with our common rules
# for the IBM format, so define these special rules to build and run extra
# executables for the IEEE binary128 format, reusing the same sources and
# inputs.
define tst-scanf-format-128ibm-compat-tests
test-srcs += \
  tst-scanf-format-$(1)-ldouble-128ibm-compat \
  # test-srcs

$$(objpfx)tst-scanf-format-$(1)-ldouble-128ibm-compat.o: \
	tst-scanf-format-$(1)-ldouble.c $$(before-compile)
	$$(compile-command.c)
$$(objpfx)tst-scanf-format-$(1)-ldouble-128ibm-compat: $$(libm)
endef
$(foreach p,$(xscanf-funcs), \
  $(eval $(call tst-scanf-format-128ibm-compat-tests,$(p))))

ifeq ($(run-built-tests),yes)
define tst-scanf-format-128ibm-compat-rule
tests-special += \
  $$(objpfx)tst-scanf-format-$(1)-ldouble-128ibm-compat-$(2).out \
  # tests-special

$$(objpfx)tst-scanf-format-$(1)-ldouble-128ibm-compat-$(2).out: \
	../sysdeps/ieee754/ldbl-128/tst-scanf-format-ldouble-$(2).input \
	$$(objpfx)tst-scanf-format-$(1)-ldouble-128ibm-compat
	$$(make-tst-scanf-format-out) > $$@ < $$<; \
	$$(evaluate-test)

ifeq (yes,$(build-shared))
ifneq ($(PERL),no)
tests-special += \
  $$(objpfx)tst-scanf-format-$(1)-ldouble-128ibm-compat-$(2)-mem.out \
  # tests-special

generated += \
  $$(objpfx)tst-scanf-format-$(1)-ldouble-128ibm-compat-$(2)-mem.out \
  $$(objpfx)tst-scanf-format-$(1)-ldouble-128ibm-compat-$(2).mtrace \
  # generated
endif
endif
endef
$(foreach f,$(fmts-xscanf-real), \
  $(foreach p,$(xscanf-funcs), \
    $(eval $(call tst-scanf-format-128ibm-compat-rule,$(p),$(f)))))
endif # $(run-built-tests) == yes
endif # $(subdir) == stdio-common

# Basic tests for the implementation of long double with IEEE binary128
# format and for the related redirections in installed headers.
tests-internal += test-printf-ieee128 test-printf-ibm128
CFLAGS-test-printf-ieee128.c += -mfloat128 -mabi=ieeelongdouble -Wno-psabi
CFLAGS-test-printf-ibm128.c += -mabi=ibmlongdouble -Wno-psabi

tests-internal += test-wprintf-ieee128 test-wprintf-ibm128
CFLAGS-test-wprintf-ieee128.c += -mfloat128 -mabi=ieeelongdouble -Wno-psabi
CFLAGS-test-wprintf-ibm128.c += -mabi=ibmlongdouble -Wno-psabi

# The following tests are setting _FORTIFY_SOURCE internally
tests-internal += test-obstack-ieee128 test-obstack-ibm128
CFLAGS-test-obstack-ieee128.c += -mfloat128 -mabi=ieeelongdouble -Wno-psabi \
				 $(no-fortify-source)
CFLAGS-test-obstack-ibm128.c += -mabi=ibmlongdouble -Wno-psabi \
				$(no-fortify-source)

# The following tests are setting _FORTIFY_SOURCE internally
tests-internal += test-obstack-chk-ieee128 test-obstack-chk-ibm128
CFLAGS-test-obstack-chk-ieee128.c += -mfloat128 -mabi=ieeelongdouble -Wno-psabi \
				     $(no-fortify-source)
CFLAGS-test-obstack-chk-ibm128.c += -mabi=ibmlongdouble -Wno-psabi \
				    $(no-fortify-source)

tests-internal += test-scanf-ieee128 test-scanf-ibm128
CFLAGS-test-scanf-ieee128.c += -mfloat128 -mabi=ieeelongdouble -Wno-psabi -std=c89 -D_GNU_SOURCE
CFLAGS-test-scanf-ibm128.c += -mabi=ibmlongdouble -Wno-psabi -std=c89 -D_GNU_SOURCE

tests-internal += test-wscanf-ieee128 test-wscanf-ibm128
CFLAGS-test-wscanf-ieee128.c += -mfloat128 -mabi=ieeelongdouble -Wno-psabi -std=c89 -D_GNU_SOURCE
CFLAGS-test-wscanf-ibm128.c += -mabi=ibmlongdouble -Wno-psabi -std=c89 -D_GNU_SOURCE

tests-internal += test-isoc99-scanf-ieee128 test-isoc99-scanf-ibm128
CFLAGS-test-isoc99-scanf-ieee128.c += -mfloat128 -mabi=ieeelongdouble -Wno-psabi -std=c99
CFLAGS-test-isoc99-scanf-ibm128.c += -mabi=ibmlongdouble -Wno-psabi -std=c99

tests-internal += test-isoc99-wscanf-ieee128 test-isoc99-wscanf-ibm128
CFLAGS-test-isoc99-wscanf-ieee128.c += -mfloat128 -mabi=ieeelongdouble -Wno-psabi -std=c99
CFLAGS-test-isoc99-wscanf-ibm128.c += -mabi=ibmlongdouble -Wno-psabi -std=c99

ifeq ($(run-built-tests),yes)
tests-special += $(objpfx)test-scanf-ieee128.out
tests-special += $(objpfx)test-scanf-ibm128.out

tests-special += $(objpfx)test-wscanf-ieee128.out
tests-special += $(objpfx)test-wscanf-ibm128.out

tests-special += $(objpfx)test-isoc99-scanf-ieee128.out
tests-special += $(objpfx)test-isoc99-scanf-ibm128.out

tests-special += $(objpfx)test-isoc99-wscanf-ieee128.out
tests-special += $(objpfx)test-isoc99-wscanf-ibm128.out
endif

$(objpfx)test-scanf-ieee128.out: \
  ../sysdeps/ieee754/ldbl-128ibm-compat/test-scanf-ldbl-compat.sh \
  $(objpfx)test-scanf-ieee128
	$(SHELL) $^ '$(test-program-prefix)' $@; \
	$(evaluate-test)

$(objpfx)test-scanf-ibm128.out: \
  ../sysdeps/ieee754/ldbl-128ibm-compat/test-scanf-ldbl-compat.sh \
  $(objpfx)test-scanf-ibm128
	$(SHELL) $^ '$(test-program-prefix)' $@; \
	$(evaluate-test)

$(objpfx)test-wscanf-ieee128.out: \
  ../sysdeps/ieee754/ldbl-128ibm-compat/test-scanf-ldbl-compat.sh \
  $(objpfx)test-wscanf-ieee128
	$(SHELL) $^ '$(test-program-prefix)' $@; \
	$(evaluate-test)

$(objpfx)test-wscanf-ibm128.out: \
  ../sysdeps/ieee754/ldbl-128ibm-compat/test-scanf-ldbl-compat.sh \
  $(objpfx)test-wscanf-ibm128
	$(SHELL) $^ '$(test-program-prefix)' $@; \
	$(evaluate-test)

$(objpfx)test-isoc99-scanf-ieee128.out: \
  ../sysdeps/ieee754/ldbl-128ibm-compat/test-scanf-ldbl-compat.sh \
  $(objpfx)test-isoc99-scanf-ieee128
	$(SHELL) $^ '$(test-program-prefix)' $@; \
	$(evaluate-test)

$(objpfx)test-isoc99-scanf-ibm128.out: \
  ../sysdeps/ieee754/ldbl-128ibm-compat/test-scanf-ldbl-compat.sh \
  $(objpfx)test-isoc99-scanf-ibm128
	$(SHELL) $^ '$(test-program-prefix)' $@; \
	$(evaluate-test)

$(objpfx)test-isoc99-wscanf-ieee128.out: \
  ../sysdeps/ieee754/ldbl-128ibm-compat/test-scanf-ldbl-compat.sh \
  $(objpfx)test-isoc99-wscanf-ieee128
	$(SHELL) $^ '$(test-program-prefix)' $@; \
	$(evaluate-test)

$(objpfx)test-isoc99-wscanf-ibm128.out: \
  ../sysdeps/ieee754/ldbl-128ibm-compat/test-scanf-ldbl-compat.sh \
  $(objpfx)test-isoc99-wscanf-ibm128
	$(SHELL) $^ '$(test-program-prefix)' $@; \
	$(evaluate-test)

tests-internal += test-printf-size-ieee128 test-printf-size-ibm128
CFLAGS-test-printf-size-ieee128.c += -mfloat128 -mabi=ieeelongdouble -Wno-psabi
CFLAGS-test-printf-size-ibm128.c += -mabi=ibmlongdouble -Wno-psabi

ifeq ($(run-built-tests),yes)
tests-special += $(objpfx)test-printf-size-ieee128.out
tests-special += $(objpfx)test-printf-size-ibm128.out
endif

$(objpfx)test-printf-size-ieee128.out: \
  tst-printfsz-islongdouble.sh $(objpfx)test-printf-size-ieee128
	$(SHELL) $^ '$(test-program-prefix)' $@; \
	$(evaluate-test)

$(objpfx)test-printf-size-ibm128.out: \
  tst-printfsz-islongdouble.sh $(objpfx)test-printf-size-ibm128
	$(SHELL) $^ '$(test-program-prefix)' $@; \
	$(evaluate-test)
endif

ifeq ($(subdir),debug)
ldbl-extra-routines += asprintf_chk \
		       dprintf_chk \
		       fprintf_chk \
		       printf_chk \
		       snprintf_chk \
		       sprintf_chk \
		       vasprintf_chk \
		       vdprintf_chk \
		       vfprintf_chk \
		       vprintf_chk \
		       vsnprintf_chk \
		       vsprintf_chk \
		       fwprintf_chk \
		       swprintf_chk \
		       wprintf_chk \
		       vfwprintf_chk \
		       vswprintf_chk \
		       vwprintf_chk \
		       obstack_chk \
		       vobstack_chk

# The following tests are setting _FORTIFY_SOURCE internally
tests-internal += test-printf-chk-ieee128 test-printf-chk-ibm128
CFLAGS-test-printf-chk-ieee128.c += -mfloat128 -mabi=ieeelongdouble -Wno-psabi \
				    $(no-fortify-source)
CFLAGS-test-printf-chk-ibm128.c += -mabi=ibmlongdouble -Wno-psabi \
				   $(no-fortify-source)

# The following tests are setting _FORTIFY_SOURCE internally
tests-internal += test-printf-chk-redir-ieee128 test-printf-chk-redir-ibm128
CFLAGS-test-printf-chk-redir-ieee128.c += -mfloat128 -mabi=ieeelongdouble \
					  -Wno-psabi -Wno-unused-result \
					  $(no-fortify-source)
CFLAGS-test-printf-chk-redir-ibm128.c += -mabi=ibmlongdouble -Wno-psabi \
					 -Wno-unused-result \
					 $(no-fortify-source)

# The following tests are setting _FORTIFY_SOURCE internally
tests-internal += test-wprintf-chk-ieee128 test-wprintf-chk-ibm128
CFLAGS-test-wprintf-chk-ieee128.c += -mfloat128 -mabi=ieeelongdouble -Wno-psabi \
				     $(no-fortify-source)
CFLAGS-test-wprintf-chk-ibm128.c += -mabi=ibmlongdouble -Wno-psabi \
				    $(no-fortify-source)

# The following tests are setting _FORTIFY_SOURCE internally
tests-internal += test-wprintf-chk-redir-ieee128 test-wprintf-chk-redir-ibm128
CFLAGS-test-wprintf-chk-redir-ieee128.c += -mfloat128 -mabi=ieeelongdouble \
					   -Wno-psabi -Wno-unused-result \
					   $(no-fortify-source)
CFLAGS-test-wprintf-chk-redir-ibm128.c += -mabi=ibmlongdouble -Wno-psabi \
					  -Wno-unused-result \
					  $(no-fortify-source)
endif

ifeq ($(subdir),argp)
ldbl-extra-routines += argp-help

$(objpfx)tst-ieee128-argp.c: tst-ldbl-argp.c
	cp $< $@

$(objpfx)tst-ibm128-argp.c: tst-ldbl-argp.c
	cp $< $@

tests-internal += tst-ieee128-argp tst-ibm128-argp
CFLAGS-tst-ieee128-argp.c += -mfloat128 -mabi=ieeelongdouble -Wno-psabi
CFLAGS-tst-ibm128-argp.c += -mabi=ibmlongdouble -Wno-psabi
endif

ifeq ($(subdir),misc)
ldbl-extra-routines += err \
		       error \
		       syslog \
		       qefgcvt \
		       qefgcvt_r

CFLAGS-ieee128-qefgcvt.c += -mabi=ieeelongdouble -Wno-psabi -mno-gnu-attribute
CFLAGS-ieee128-qefgcvt_r.c += -mabi=ieeelongdouble -Wno-psabi -mno-gnu-attribute

tests-internal += tst-ibm128-warn tst-ieee128-warn
tests-internal += tst-ibm128-error tst-ieee128-error
tests-internal += tst-ibm128-efgcvt tst-ieee128-efgcvt
tests-internal += tst-ieee128-errorfptr

$(objpfx)tst-ibm128-%.c: tst-ldbl-%.c
	cp $< $@

$(objpfx)tst-ieee128-%.c: tst-ldbl-%.c
	cp $< $@

CFLAGS-tst-ibm128-warn.c += -mabi=ibmlongdouble -Wno-psabi
CFLAGS-tst-ibm128-error.c += -mabi=ibmlongdouble -Wno-psabi
CFLAGS-tst-ibm128-efgcvt.c += -mabi=ibmlongdouble -Wno-psabi

CFLAGS-tst-ieee128-warn.c += -mfloat128 -mabi=ieeelongdouble -Wno-psabi
CFLAGS-tst-ieee128-error.c += -mfloat128 -mabi=ieeelongdouble -Wno-psabi
CFLAGS-tst-ieee128-efgcvt.c += -mfloat128 -mabi=ieeelongdouble -Wno-psabi
CFLAGS-tst-ieee128-errorfptr.c += -mfloat128 -mabi=ieeelongdouble -Wno-psabi

# The following tests are setting _FORTIFY_SOURCE internally
tests-container += test-syslog-ieee128 test-syslog-ibm128
CFLAGS-test-syslog-ieee128.c += -mfloat128 -mabi=ieeelongdouble -Wno-psabi \
				$(no-fortify-source)
CFLAGS-test-syslog-ibm128.c += -mabi=ibmlongdouble -Wno-psabi \
			       $(no-fortify-source)

# The following tests are setting _FORTIFY_SOURCE internally
tests-container += test-syslog-chk-ieee128 test-syslog-chk-ibm128
CFLAGS-test-syslog-chk-ieee128.c += -mfloat128 -mabi=ieeelongdouble -Wno-psabi \
				    $(no-fortify-source)
CFLAGS-test-syslog-chk-ibm128.c += -mabi=ibmlongdouble -Wno-psabi \
				   $(no-fortify-source)
endif

ifeq ($(subdir),stdlib)
ldbl-extra-routines += strfmon strfmon_l

# Printing long double values with IEEE binary128 format reuses part
# of the internal float128 implementation (__printf_fp, __printf_fphex,
# and __float128 variables and union members).  Thus, the compilation of
# the following files, must have -mfloat128 passed to the compiler.
# Also, guarantee that they are compiled in IBM long double mode.
CFLAGS-strfmon.c += -mfloat128 -mabi=ibmlongdouble
CFLAGS-strfmon_l.c += -mfloat128 -mabi=ibmlongdouble

tests-internal += test-strfmon-ibm128 test-strfmon-ieee128
CFLAGS-test-strfmon-ibm128.c += -mabi=ibmlongdouble -Wno-psabi
CFLAGS-test-strfmon-ieee128.c += -mfloat128 -mabi=ieeelongdouble -Wno-psabi
$(objpfx)tst-strfmon-ibm128.out: $(gen-locales)
$(objpfx)tst-strfmon-ieee128.out: $(gen-locales)

tests-internal += test-strfrom-ibm128 test-strfrom-ieee128
CFLAGS-test-strfrom-ieee128.c += -mfloat128 -mabi=ieeelongdouble -Wno-psabi
CFLAGS-test-strfrom-ibm128.c += -mabi=ibmlongdouble -Wno-psabi

endif

ifeq ($(subdir),math)
# The totalorder{,mag} compatibility tests were added prior to this
# support.  Thus, they can only correctly be used with ibm128.
CFLAGS-test-ldouble-compat_totalorder.c += -mabi=ibmlongdouble
CFLAGS-test-ldouble-compat_totalordermag.c += -mabi=ibmlongdouble

# The compatibility tests for fromfp functions access particular
# versioned symbols by name, and those symbols are for the ibm128
# format.  (It would also be possible to test the *ieee128 compat
# symbols, but these tests don't do so.)
CFLAGS-test-ldouble-compat_fromfp.c += -mabi=ibmlongdouble
CFLAGS-test-ldouble-compat_fromfpx.c += -mabi=ibmlongdouble
CFLAGS-test-ldouble-compat_ufromfp.c += -mabi=ibmlongdouble
CFLAGS-test-ldouble-compat_ufromfpx.c += -mabi=ibmlongdouble
endif

ifeq ($(subdir), stdio-common)
tests += tst-ibm128-scanf-binary-c11 \
	 tst-ibm128-scanf-binary-c23 \
	 tst-ibm128-scanf-binary-gnu11 \
	 tst-ibm128-scanf-binary-gnu89 \
	 tst-ieee128-scanf-binary-c11 \
	 tst-ieee128-scanf-binary-c23 \
	 tst-ieee128-scanf-binary-gnu11 \
	 tst-ieee128-scanf-binary-gnu89

# Some versions of GCC supported for building glibc do not support -std=c23
# (added in GCC 14), or the older name -std=c2x (added in GCC 9), so
# the test for that version uses -std=c11 and then _ISOC23_SOURCE is defined in
# the test as needed.
CFLAGS-tst-ibm128-scanf-binary-c11.c += -mabi=ibmlongdouble -Wno-psabi \
					-std=c11 -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-ibm128-scanf-binary-c23.c += -mabi=ibmlongdouble -Wno-psabi \
					-std=c11 -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-ibm128-scanf-binary-gnu11.c += -mabi=ibmlongdouble -Wno-psabi \
					  -std=gnu11 -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-ibm128-scanf-binary-gnu89.c += -mabi=ibmlongdouble -Wno-psabi \
					  -std=gnu89 -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-ieee128-scanf-binary-c11.c += -mfloat128 -mabi=ieeelongdouble \
					 -Wno-psabi -std=c11 \
					 -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-ieee128-scanf-binary-c23.c += -mfloat128 -mabi=ieeelongdouble \
					 -Wno-psabi -std=c11 \
					 -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-ieee128-scanf-binary-gnu11.c += -mfloat128 -mabi=ieeelongdouble \
					   -Wno-psabi -std=gnu11 \
					   -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-ieee128-scanf-binary-gnu89.c += -mfloat128 -mabi=ieeelongdouble \
					   -Wno-psabi -std=gnu89 \
					   -DOBJPFX=\"$(objpfx)\"

endif

ifeq ($(subdir), wcsmbs)
tests += tst-ibm128-wscanf-binary-c11 \
	 tst-ibm128-wscanf-binary-c23 \
	 tst-ibm128-wscanf-binary-gnu11 \
	 tst-ibm128-wscanf-binary-gnu89 \
	 tst-ieee128-wscanf-binary-c11 \
	 tst-ieee128-wscanf-binary-c23 \
	 tst-ieee128-wscanf-binary-gnu11 \
	 tst-ieee128-wscanf-binary-gnu89

# Some versions of GCC supported for building glibc do not support -std=c23
# (added in GCC 14), or the older name -std=c2x (added in GCC 9), so
# the test for that version uses -std=c11 and then _ISOC23_SOURCE is defined in
# the test as needed.
CFLAGS-tst-ibm128-wscanf-binary-c11.c += -mabi=ibmlongdouble -Wno-psabi \
					 -std=c11 -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-ibm128-wscanf-binary-c23.c += -mabi=ibmlongdouble -Wno-psabi \
					 -std=c11 -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-ibm128-wscanf-binary-gnu11.c += -mabi=ibmlongdouble -Wno-psabi \
					   -std=gnu11 -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-ibm128-wscanf-binary-gnu89.c += -mabi=ibmlongdouble -Wno-psabi \
					   -std=gnu89 -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-ieee128-wscanf-binary-c11.c += -mfloat128 -mabi=ieeelongdouble \
					  -Wno-psabi -std=c11 \
					  -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-ieee128-wscanf-binary-c23.c += -mfloat128 -mabi=ieeelongdouble \
					  -Wno-psabi -std=c11 \
					  -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-ieee128-wscanf-binary-gnu11.c += -mfloat128 -mabi=ieeelongdouble \
					    -Wno-psabi -std=gnu11 \
					    -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-ieee128-wscanf-binary-gnu89.c += -mfloat128 -mabi=ieeelongdouble \
					    -Wno-psabi -std=gnu89 \
					    -DOBJPFX=\"$(objpfx)\"

endif

# Add IEEE binary128 files as make targets.
routines += $(foreach r,$(ldbl-extra-routines),ieee128-$(r))

routines_no_fortify += \
  ieee128-asprintf \
  ieee128-dprintf \
  ieee128-fprintf \
  ieee128-fwprintf \
  ieee128-obstack \
  ieee128-printf \
  ieee128-snprintf \
  ieee128-sprintf \
  ieee128-swprintf \
  ieee128-syslog \
  ieee128-vasprintf \
  ieee128-vdprintf \
  ieee128-vfprintf \
  ieee128-vfwprintf \
  ieee128-vobstack \
  ieee128-vprintf \
  ieee128-vsnprintf \
  ieee128-vsprintf \
  ieee128-vswprintf \
  ieee128-vsyslog \
  ieee128-vwprintf \
  ieee128-wprintf \
  # routines_no_fortify

# On powerpc64le, the routines responsible for converting between long
# double and string (e.g.: printf, scanf, strtold, strfroml) default to
# IBM long double mode.  When support for TS 18661-3 was added, several
# internal functions were modified so that they could also handle
# floating-point variables with IEEE binary128 format, but as an
# additional type, _Float128.
#
# The following rule ensures that the string conversion routines will
# always be built in IBM long double mode, with additional support for
# IEEE binary128, through the use of -mabi=ibmlongdouble and -mfloat128.
ldbl-ibm128-files := $(objpfx)test-%-ibm128^ \
		     $(objpfx)iovdprintf^ \
		     $(objpfx)iovsprintf^ \
		     $(objpfx)iovsscanf^ \
		     $(objpfx)iovswscanf^ \
		     $(objpfx)iovfscanf^ \
		     $(objpfx)mpn2ldbl^ \
		     $(objpfx)ldbl2mpn^ \
		     $(objpfx)strtold_nan^ \
		     $(objpfx)strtold_l^ \
		     $(objpfx)strtold^ \
		     $(objpfx)strfroml^ \
		     $(objpfx)wcstold_nan^ \
		     $(objpfx)wcstold_l^ \
		     $(objpfx)wcstold^ \
		     $(objpfx)printf_fphex^ \
		     $(objpfx)printf_fp^ \
		     $(foreach r,$(ldbl-extra-routines),$(objpfx)$(r)^) \
		     $(foreach r,$(ldbl-extra-routines),$(objpfx)$(r)-internal^)
obj-suf-foreach = $(foreach suf,$(all-object-suffixes),$(subst ^,$(suf),$(1)))

# Explicitly add -mabi=ibm-long-double to required files.
$(call obj-suf-foreach,$(ldbl-ibm128-files)): \
  sysdep-CFLAGS += -mabi=ibmlongdouble
