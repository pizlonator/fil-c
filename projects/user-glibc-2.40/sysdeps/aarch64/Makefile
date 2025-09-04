long-double-fcts = yes

ifeq (yes,$(aarch64-bti))
# Mark linker output BTI compatible, it warns on non-BTI inputs.
sysdep-LDFLAGS += -Wl,-z,force-bti
# Make warnings fatal outside the test system.
LDFLAGS-lib.so += -Wl,--fatal-warnings
LDFLAGS-rtld += -Wl,-z,force-bti,--fatal-warnings
endif

ifeq ($(subdir),elf)
sysdep-dl-routines += dl-bti

tests += tst-audit26 \
	 tst-audit27

modules-names += \
    tst-audit26mod \
    tst-auditmod26 \
    tst-audit27mod \
    tst-auditmod27

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
sysdep-rtld-routines += dl-start
sysdep-dl-routines += tlsdesc dl-tlsdesc
gen-as-const-headers += dl-link.sym

tests-internal += tst-ifunc-arg-1 tst-ifunc-arg-2

ifeq (yes,$(aarch64-variant-pcs))
tests += tst-vpcs
modules-names += tst-vpcs-mod
LDFLAGS-tst-vpcs-mod.so = -Wl,-z,lazy
$(objpfx)tst-vpcs: $(objpfx)tst-vpcs-mod.so
endif
endif

ifeq ($(subdir),csu)
gen-as-const-headers += \
  tlsdesc.sym \
  rtld-global-offsets.sym
endif

ifeq ($(subdir),gmon)
CFLAGS-mcount.c += -mgeneral-regs-only
endif

ifeq ($(subdir),math)
CPPFLAGS += -I../soft-fp
CFLAGS-feupdateenv.c += $(config-cflags-wno-ignored-attributes)
endif

ifeq ($(subdir),misc)
sysdep_headers += sys/ifunc.h
sysdep_routines += \
  __mtag_tag_zero_region \
  __mtag_tag_region \
  __arm_za_disable

tests += \
  tst-sme-jmp
endif

ifeq ($(subdir),malloc)
sysdep_malloc_debug_routines = __mtag_tag_zero_region __mtag_tag_region
endif
