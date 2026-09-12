# The Sparc `long double' is a distinct type we support.
long-double-fcts = yes

ifeq ($(subdir),gmon)
sysdep_routines += sparc-mcount
endif

ifeq ($(subdir),csu)
CPPFLAGS-crti.S += -fPIC
CPPFLAGS-crtn.S += -fPIC
endif

ifeq ($(subdir),elf)

# Lazy binding on SPARC rewrites the PLT sequence.  See the Solaris
# Linker and Libraries Guide, section SPARC: Procedure Linkage Table.
# <https://docs.oracle.com/cd/E19455-01/816-0559/chapter6-1236/index.html>
test-xfail-check-wx-segment = *

endif # $(subdir) == elf

# nscd uses atomic_spin_nop which in turn requires cpu_relax
ifeq ($(subdir),nscd)
routines += cpu_relax
endif

# The assembler on SPARC needs the -fPIC flag even when it's assembler code.
ASFLAGS-.os += -fPIC

# When PIE is enabled by default, the regular (.o) objects are also built as
# PIC, as $(pic-default) adds -DPIC to CPPFLAGS-.o.  That makes the SPARC
# assembler sources use the PIC GOT setup sequence, which refers to
# # _GLOBAL_OFFSET_TABLE_ through %hi/%lo.  The assembler only turns those into
# the required PC-relative relocations (R_SPARC_PC22/PC10) when it is in PIC
# mode; without -fPIC it emits absolute R_SPARC_HI22/LO10 instead, so the
# run-time GOT register is computed incorrectly.
ifeq (yes,$(build-pie-default))
ASFLAGS-.o += -fPIC
endif
