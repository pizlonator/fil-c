/* Covers zsys_dl_find_object: it walks the loaded-object program headers (via the runtime's
   dl_iterate_phdr) to find the object whose PT_LOAD segments cover an address, and reports that
   object's mapping range and PT_GNU_EH_FRAME in a Fil-C struct dl_find_object. The opaque address
   fields are forged the same way as zsys_dladdr's dli_fbase, so we read their integer values but
   never dereference them. */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <link.h>
#include <elf.h>
#include <stdint.h>
#include <stdio.h>
#include <stdfil.h>

/* No public header declares static_dl_find_object, so reference the exported symbol directly to
   call it by name below. */
extern int static_dl_find_object(void *address, struct dl_find_object *result);

struct other {
    /* Main program's mapping range, which pick_other_cb excludes when choosing another object. */
    uintptr_t main_lo, main_hi;
    /* Address inside a non-main object for the cross-object check, or 0 until one is found. */
    uintptr_t addr;
};

/* Finds an address guaranteed to live inside a different object than the main program, to feed the
   cross-object dl_find_object check below.

   Objects are told apart by mapping range, not by name: skip any object whose first PT_LOAD address
   falls inside main's own mapping and take the first other object's. Sourcing the address from
   dl_iterate_phdr means the test never guesses, since the runtime itself reported it as mapped.
   Selecting by name would be wrong, because under Fil-C the main program's dlpi_name is not empty. */
static int pick_other_cb(struct dl_phdr_info* info, size_t size, void* data)
{
    (void)size;
    struct other* o = (struct other*)data;
    /* Already have one. */
    if (o->addr)
        return 0;
    unsigned i;
    for (i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr)* ph = info->dlpi_phdr + i;
        if (ph->p_type == PT_LOAD) {
            uintptr_t a = (uintptr_t)info->dlpi_addr + (uintptr_t)ph->p_vaddr;
            /* This one is the main program, so skip it. */
            if (a >= o->main_lo && a < o->main_hi)
                return 0;
            /* Found another object's loaded segment, so record it and stop. */
            o->addr = a;
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    /* Resolve the object that contains main() itself. main's code lives in a PT_LOAD segment of
       the main program, so this must succeed. */
    struct dl_find_object fo;
    int r = _dl_find_object((void*)&main, &fo);
    ZASSERT(r == 0);

    uintptr_t addr = (uintptr_t)(void*)&main;
    uintptr_t lo = (uintptr_t)fo.dlfo_map_start;
    uintptr_t hi = (uintptr_t)fo.dlfo_map_end;

    /* The reported mapping must be a non-empty range that actually covers the queried address. */
    ZASSERT(lo != 0);
    ZASSERT(hi > lo);
    ZASSERT(addr >= lo);
    ZASSERT(addr < hi);

    /* The object's opaque handle (its load base) is reported. */
    ZASSERT(fo.dlfo_link_map != 0);

    /* Fields the runtime writes explicitly: flags cleared, no sframe. */
    ZASSERT(fo.dlfo_flags == 0);
    ZASSERT(fo.dlfo_sframe == 0);

    /* Checks cross-object resolution: an address inside a different object must resolve to a
       different mapping, not always report the main program.

       Source the address from dl_iterate_phdr so it is known to be mapped; a Fil-C program always
       loads libc.so and libpizlo.so, so at least one other object is present. */
    struct other o;
    o.main_lo = lo;
    o.main_hi = hi;
    o.addr = 0;
    dl_iterate_phdr(pick_other_cb, &o);
    ZASSERT(o.addr != 0);
    struct dl_find_object fo2;
    int r3 = _dl_find_object((void*)o.addr, &fo2);
    ZASSERT(r3 == 0);
    uintptr_t lo2 = (uintptr_t)fo2.dlfo_map_start;
    uintptr_t hi2 = (uintptr_t)fo2.dlfo_map_end;
    ZASSERT(hi2 > lo2);
    /* The found range covers the queried address. */
    ZASSERT(o.addr >= lo2 && o.addr < hi2);
    /* And it is a genuinely different object than main. */
    ZASSERT(lo2 != lo);

    /* Resolve &main through static_dl_find_object and require the identical mapping as
       _dl_find_object, proving the static_ entry point works and not only its weak alias. */
    struct dl_find_object fo_s;
    int rs = static_dl_find_object((void*)&main, &fo_s);
    ZASSERT(rs == 0);
    ZASSERT((uintptr_t)fo_s.dlfo_map_start == lo);
    ZASSERT((uintptr_t)fo_s.dlfo_map_end == hi);

    /* An address that belongs to no loaded object (a heap/stack address is not inside any object's
       PT_LOAD range) must report "not found". */
    struct dl_find_object miss;
    int r2 = _dl_find_object((void*)&fo, &miss);
    ZASSERT(r2 == -1);

    printf("dl_find_object resolved main in [%p, %p).\n", fo.dlfo_map_start, fo.dlfo_map_end);
    return 0;
}
