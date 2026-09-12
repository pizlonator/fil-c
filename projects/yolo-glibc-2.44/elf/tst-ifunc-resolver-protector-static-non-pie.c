/* Same coverage as tst-ifunc-resolver-protector-static, but linked as non-PIE
   static.  Exercises the apply_irel / __rela_iplt_start path instead of the
   static-pie _dl_relocate_static_pie_ifunc path.  */

#include "tst-ifunc-resolver-protector-static.c"
