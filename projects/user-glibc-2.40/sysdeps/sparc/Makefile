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
