ifeq ($(subdir),stdio-common)
ifeq ($(run-built-tests),yes)
# Put long double IEEE binary128 format inputs in sysdeps/ieee754/ldbl-128
# first in vpath.  Otherwise the ordering of sysdeps would cause long double
# IEEE binary64 format inputs in sysdeps/ieee754/ldbl-opt to be used instead
# for the common rules requested in sysdeps/ieee754/ldbl-128 with targets
# using the dual -mlong-double-128/-mlong-double-64 arrangement.
define tst-scanf-format-64-128-vpath
vpath tst-scanf-format-ldouble-$(1).input ../sysdeps/ieee754/ldbl-128
endef
$(foreach f,$(fmts-xscanf-real), \
  $(eval $(call tst-scanf-format-64-128-vpath,$(f))))
endif # $(run-built-tests) == yes
endif # $(subdir) == stdio-common
