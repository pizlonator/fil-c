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
sysdep-rtld-routines += \
  dl-start \
  # sysdep-rtld-routines
sysdep-dl-routines += \
  dl-tlsdesc \
  tlsdesc \
  # sysdep-dl-routines
gen-as-const-headers += \
  dl-link.sym \
  rtld-global-offsets.sym \
  # gen-as-const-headers

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
gen-as-const-headers += \
  rtld-global-offsets.sym \
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
sysdep_routines += \
  __alloc_gcs \
  __arm_za_disable \
  __mte_tag_region \
  __mte_tag_region_zero \
  # sysdep_routines

tests += \
  tst-sme-jmp \
  tst-sme-signal \
  tst-sme-za-state \
  # tests
tests-internal += \
  tst-sme-clone \
  tst-sme-clone3 \
  tst-sme-fork \
  tst-sme-vfork \
  tst-sme-za-disable-fail \
  # tests-internal

$(objpfx)tst-sme-clone3: $(objpfx)clone3.o $(objpfx)__arm_za_disable.o
$(objpfx)tst-sme-za-disable-fail: $(objpfx)__arm_za_disable.o
endif

ifeq ($(subdir),malloc)

endif # malloc directory

ifeq ($(subdir),support)
libsupport-sysdep_routines += \
  support-address-diff \
  support-use-after-free \
  # libsupport-sysdep_routines
endif
