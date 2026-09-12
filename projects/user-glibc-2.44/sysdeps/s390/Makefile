ifeq ($(subdir),gmon)
sysdep_routines += \
  s390x-mcount \
  # sysdep_routines
endif

ifeq ($(subdir),iconvdata)
ISO-8859-1_CP037_Z900-routines := iso-8859-1_cp037_z900
ISO-8859-1_CP037_Z900-map := gconv.map

UTF8_UTF32_Z9-routines := utf8-utf32-z9
UTF8_UTF32_Z9-map := gconv.map

UTF16_UTF32_Z9-routines := utf16-utf32-z9
UTF16_UTF32_Z9-map := gconv.map

UTF8_UTF16_Z9-routines := utf8-utf16-z9
UTF8_UTF16_Z9-map := gconv.map

s390x-iconv-modules = \
  ISO-8859-1_CP037_Z900 \
  UTF16_UTF32_Z9 \
  UTF8_UTF16_Z9 \
  UTF8_UTF32_Z9 \
  # s390x-iconv-modules

extra-modules-left += $(s390x-iconv-modules)
include extra-module.mk

cpp-srcs-left := $(foreach mod,$(s390x-iconv-modules),$($(mod)-routines))
lib := iconvdata
include $(patsubst %,$(..)libof-iterator.mk,$(cpp-srcs-left))

extra-objs      += $(addsuffix .so, $(s390x-iconv-modules))
install-others  += $(patsubst %, $(inst_gconvdir)/%.so, \
				 $(s390x-iconv-modules)) \
		   $(inst_gconvdir)/gconv-modules.d/gconv-modules-s390.conf

$(patsubst %, $(inst_gconvdir)/%.so, $(s390x-iconv-modules)) : \
$(inst_gconvdir)/%.so: $(objpfx)%.so $(+force)
	$(do-install-program)

ifdef objpfx
generated += \
  gconv-modules.d/gconv-modules-s390.conf \
  # generated
endif

$(inst_gconvdir)/gconv-modules.d/gconv-modules-s390.conf: \
		$(modpfx)gconv-modules-s390.conf $(+force)
	$(do-install)

$(modpfx)gconv-modules-s390.conf: ../sysdeps/s390/gconv-modules-s390.conf \
				  $(modpfx)
	cp $< $@
endif

ifeq ($(subdir),elf)
$(objpfx)tst-glibc-hwcaps: \
  $(objpfx)libmarkermod2-1.so \
  $(objpfx)libmarkermod3-1.so \
  $(objpfx)libmarkermod4-1.so \
  $(objpfx)libmarkermod5-1.so \
  $(objpfx)libmarkermod6-1.so
$(objpfx)tst-glibc-hwcaps.out: \
  $(objpfx)libmarkermod2.so \
    $(objpfx)glibc-hwcaps/z13/libmarkermod2.so \
  $(objpfx)libmarkermod3.so \
    $(objpfx)glibc-hwcaps/z13/libmarkermod3.so \
    $(objpfx)glibc-hwcaps/z14/libmarkermod3.so \
  $(objpfx)libmarkermod4.so \
    $(objpfx)glibc-hwcaps/z13/libmarkermod4.so \
    $(objpfx)glibc-hwcaps/z14/libmarkermod4.so \
    $(objpfx)glibc-hwcaps/z15/libmarkermod4.so \
  $(objpfx)libmarkermod5.so \
    $(objpfx)glibc-hwcaps/z13/libmarkermod5.so \
    $(objpfx)glibc-hwcaps/z14/libmarkermod5.so \
    $(objpfx)glibc-hwcaps/z15/libmarkermod5.so \
    $(objpfx)glibc-hwcaps/z16/libmarkermod5.so \
  $(objpfx)libmarkermod6.so \
    $(objpfx)glibc-hwcaps/z13/libmarkermod6.so \
    $(objpfx)glibc-hwcaps/z14/libmarkermod6.so \
    $(objpfx)glibc-hwcaps/z15/libmarkermod6.so \
    $(objpfx)glibc-hwcaps/z16/libmarkermod6.so \
    $(objpfx)glibc-hwcaps/z17/libmarkermod6.so


$(objpfx)glibc-hwcaps/z13/libmarkermod2.so: $(objpfx)libmarkermod2-2.so
	$(make-target-directory)
	cp $< $@
$(objpfx)glibc-hwcaps/z13/libmarkermod3.so: $(objpfx)libmarkermod3-2.so
	$(make-target-directory)
	cp $< $@
$(objpfx)glibc-hwcaps/z14/libmarkermod3.so: $(objpfx)libmarkermod3-3.so
	$(make-target-directory)
	cp $< $@
$(objpfx)glibc-hwcaps/z13/libmarkermod4.so: $(objpfx)libmarkermod4-2.so
	$(make-target-directory)
	cp $< $@
$(objpfx)glibc-hwcaps/z14/libmarkermod4.so: $(objpfx)libmarkermod4-3.so
	$(make-target-directory)
	cp $< $@
$(objpfx)glibc-hwcaps/z15/libmarkermod4.so: $(objpfx)libmarkermod4-4.so
	$(make-target-directory)
	cp $< $@
$(objpfx)glibc-hwcaps/z13/libmarkermod5.so: $(objpfx)libmarkermod5-2.so
	$(make-target-directory)
	cp $< $@
$(objpfx)glibc-hwcaps/z14/libmarkermod5.so: $(objpfx)libmarkermod5-3.so
	$(make-target-directory)
	cp $< $@
$(objpfx)glibc-hwcaps/z15/libmarkermod5.so: $(objpfx)libmarkermod5-4.so
	$(make-target-directory)
	cp $< $@
$(objpfx)glibc-hwcaps/z16/libmarkermod5.so: $(objpfx)libmarkermod5-5.so
	$(make-target-directory)
	cp $< $@
$(objpfx)glibc-hwcaps/z13/libmarkermod6.so: $(objpfx)libmarkermod6-2.so
	$(make-target-directory)
	cp $< $@
$(objpfx)glibc-hwcaps/z14/libmarkermod6.so: $(objpfx)libmarkermod6-3.so
	$(make-target-directory)
	cp $< $@
$(objpfx)glibc-hwcaps/z15/libmarkermod6.so: $(objpfx)libmarkermod6-4.so
	$(make-target-directory)
	cp $< $@
$(objpfx)glibc-hwcaps/z16/libmarkermod6.so: $(objpfx)libmarkermod6-5.so
	$(make-target-directory)
	cp $< $@
$(objpfx)glibc-hwcaps/z17/libmarkermod6.so: $(objpfx)libmarkermod6-6.so
	$(make-target-directory)
	cp $< $@


ifeq (no,$(build-hardcoded-path-in-tests))
# This is an ld.so.cache test, and RPATH/RUNPATH in the executable
# interferes with its test objectives.
tests-container += \
  tst-glibc-hwcaps-cache \
  # tests-container
endif

ifeq ($(build-shared),yes)
tests += \
  tst-dl-runtime-profile-noaudit \
  tst-dl-runtime-resolve-audit \
  tst-dl-runtime-resolve-noaudit \
  # tests
# FIXME: When both LD_PROFILE and LD_AUDIT are set, a segfault occurs.
#tests += tst-dl-runtime-profile-audit
modules-names += \
  tst-dl-runtime-mod \
  # modules-names
$(objpfx)tst-dl-runtime-resolve-noaudit: $(objpfx)tst-dl-runtime-mod.so
$(objpfx)tst-dl-runtime-resolve-audit: $(objpfx)tst-dl-runtime-mod.so
$(objpfx)tst-dl-runtime-profile-noaudit: $(objpfx)tst-dl-runtime-mod.so
$(objpfx)tst-dl-runtime-profile-audit: $(objpfx)tst-dl-runtime-mod.so
$(objpfx)tst-dl-runtime-resolve-audit.out: $(objpfx)tst-auditmod1.so
$(objpfx)tst-dl-runtime-profile-audit.out: $(objpfx)tst-auditmod1.so
# The profiling output goes to $LD_PROFILE_OUTPUT/$LD_PROFILE.profile
env-profile = LD_PROFILE=$(objpfx)tst-dl-runtime-mod.so \
	LD_PROFILE_OUTPUT=/
env-audit = LD_AUDIT=$(objpfx)tst-auditmod1.so
tst-dl-runtime-resolve-audit-ENV = $(env-audit)
tst-dl-runtime-profile-noaudit-ENV = $(env-profile)
tst-dl-runtime-profile-audit-ENV = $(env-profile) $(env-audit)
endif
endif

ifeq ($(subdir),string)
sysdep_routines += \
  memccpy \
  memccpy-c \
  memccpy-vx \
  memchr \
  memchr-vx \
  memchr-z900 \
  memcmp \
  memcmp-z900 \
  memcpy \
  memcpy-z900 \
  memmem \
  memmem-arch13 \
  memmem-c \
  memmem-vx \
  memmove \
  memmove-c \
  mempcpy \
  memrchr \
  memrchr-c \
  memrchr-vx \
  memset \
  memset-z900 \
  rawmemchr \
  rawmemchr-c \
  rawmemchr-vx \
  stpcpy \
  stpcpy-c \
  stpcpy-vx \
  stpncpy \
  stpncpy-c \
  stpncpy-vx \
  strcat \
  strcat-c \
  strcat-vx \
  strchr \
  strchr-c \
  strchr-vx \
  strchrnul \
  strchrnul-c \
  strchrnul-vx \
  strcmp \
  strcmp-vx \
  strcmp-z900 \
  strcpy \
  strcpy-vx \
  strcpy-z900 \
  strcspn \
  strcspn-c \
  strcspn-vx \
  strlen \
  strlen-c \
  strlen-vx \
  strncat \
  strncat-c \
  strncat-vx \
  strncmp \
  strncmp-c \
  strncmp-vx \
  strncpy \
  strncpy-vx \
  strncpy-z900 \
  strnlen \
  strnlen-c \
  strnlen-vx \
  strpbrk \
  strpbrk-c \
  strpbrk-vx \
  strrchr \
  strrchr-c \
  strrchr-vx \
  strspn \
  strspn-c \
  strspn-vx \
  strstr \
  strstr-arch13 \
  strstr-c \
  strstr-vx \
  # sysdep_routines

# Exclude fortified routines from being built with _FORTIFY_SOURCE
routines_no_fortify += \
  memmove-c \
  stpcpy-c \
  stpncpy-c \
  strcat-c \
  strncat-c \
  # routines_no_fortify
endif

ifeq ($(subdir),wcsmbs)
sysdep_routines += \
  wcpcpy \
  wcpcpy-c \
  wcpcpy-vx \
  wcpncpy \
  wcpncpy-c \
  wcpncpy-vx \
  wcscat \
  wcscat-c \
  wcscat-vx \
  wcschr \
  wcschr-c \
  wcschr-vx \
  wcschrnul \
  wcschrnul-c \
  wcschrnul-vx \
  wcscmp \
  wcscmp-c \
  wcscmp-vx \
  wcscpy \
  wcscpy-c \
  wcscpy-vx \
  wcscspn \
  wcscspn-c \
  wcscspn-vx \
  wcslen \
  wcslen-c \
  wcslen-vx \
  wcsncat \
  wcsncat-c \
  wcsncat-vx \
  wcsncmp \
  wcsncmp-c \
  wcsncmp-vx \
  wcsncpy \
  wcsncpy-c \
  wcsncpy-vx \
  wcsnlen \
  wcsnlen-c \
  wcsnlen-vx \
  wcspbrk \
  wcspbrk-c \
  wcspbrk-vx \
  wcsrchr \
  wcsrchr-c \
  wcsrchr-vx \
  wcsspn \
  wcsspn-c \
  wcsspn-vx \
  wmemchr \
  wmemchr-c \
  wmemchr-vx \
  wmemcmp \
  wmemcmp-c \
  wmemcmp-vx \
  wmemset \
  wmemset-c \
  wmemset-vx \
  # sysdep_routines

# Exclude fortified routines from being built with _FORTIFY_SOURCE
routines_no_fortify += \
  wcpcpy-c \
  wcpncpy-c \
  wcscat-c \
  wcscpy-c \
  wcsncat-c \
  wcsncpy-c \
  wmemset-c \
  # routines_no_fortify
endif
