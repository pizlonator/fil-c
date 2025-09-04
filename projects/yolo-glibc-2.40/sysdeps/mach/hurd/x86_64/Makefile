ifeq ($(subdir),conform)
# For bug 23090
# (missing SA_NOCLDWAIT)
conformtest-xfail-conds += x86_64-gnu
endif

ifeq ($(subdir),stdlib)
sysdep_routines += __start_context
endif
