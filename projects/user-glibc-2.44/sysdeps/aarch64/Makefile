long-double-fcts = yes

ifeq ($(subdir),elf)
sysdep-dl-routines += \
  dl-bti \
  dl-gcs \
  # sysdep-dl-routines

tests += \
  tst-audit26 \
  tst-audit27 \
  # tests

modules-names += \
  tst-audit26mod \
  tst-audit27mod \
  tst-auditmod26 \
  tst-auditmod27 \
  # modules-names

$(objpfx)tst-audit26: $(objpfx)tst-audit26mod.so \
		      $(objpfx)tst-auditmod26.so
LDFLAGS-tst-audit26 += -Wl,-z,lazy
tst-audit26-ENV = LD_AUDIT=$(objpfx)tst-auditmod26.so

$(objpfx)tst-audit27: $(objpfx)tst-audit27mod.so \
		      $(objpfx)tst-auditmod27.so
$(objpfx)tst-audit27mod.so: $(libsupport)
LDFLAGS-tst-audit27 += -Wl,-z,lazy
tst-audit27-ENV = LD_AUDIT=$(objpfx)tst-auditmod27.so
endif

ifeq ($(subdir),elf)
# Fil-C: rtld-global-offsets.sym and dl-link.sym are dropped (they compute
# offsets inside struct rtld_global / struct rtld_global_ro for ld.so-only
# assembly like dl-tlsdesc.S and dl-link.S, and the ported ldsodefs.h no
# longer defines those structs; the ld.so assembly is not built anyway).

tests-internal += \
  tst-ifunc-arg-1 \
  tst-ifunc-arg-2 \
  tst-ifunc-arg-3 \
  tst-ifunc-arg-4 \
  # tests-internal

tests += \
  tst-vpcs \
  # tests
modules-names += \
  tst-vpcs-mod \
  # modules-names
LDFLAGS-tst-vpcs-mod.so = -Wl,-z,lazy
$(objpfx)tst-vpcs: $(objpfx)tst-vpcs-mod.so
endif

ifeq ($(subdir),csu)
# Fil-C: rtld-global-offsets.sym is dropped (upstream it computes
# GLRO offsets inside struct rtld_global_ro for dl-tlsdesc.S / other ld.so
# asm, and the ported ldsodefs.h no longer defines struct rtld_global_ro;
# none of the ld.so asm is built anyway).  tlsdesc.sym is kept since
# tlsdesc.h is included by built code.
gen-as-const-headers += \
  tlsdesc.sym \
  # gen-as-const-headers
endif

ifeq ($(subdir),gmon)
CFLAGS-mcount.c += -mgeneral-regs-only
endif

ifeq ($(subdir),math)
CPPFLAGS += -I../soft-fp
endif

ifeq ($(subdir),misc)
sysdep_headers += \
  sys/ifunc.h \
  # sysdep_headers
# Fil-C: __alloc_gcs, __arm_za_disable, __mte_tag_region and
# __mte_tag_region_zero are only needed by makecontext/setcontext, which are
# provided by the Fil-C runtime's fiber context code (stdlib/makecontext.c
# etc.); the raw-asm and raw-syscall implementations are not built.

tests += \
  tst-sme-jmp \
  tst-sme-signal \
  tst-sme-za-state \
  # tests
endif

ifeq ($(subdir),malloc)

endif # malloc directory

ifeq ($(subdir),support)
libsupport-sysdep_routines += \
  support-address-diff \
  support-use-after-free \
  # libsupport-sysdep_routines
endif
