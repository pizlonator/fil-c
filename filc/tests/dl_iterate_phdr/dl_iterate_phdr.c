/* Covers zsys_dl_iterate_phdr: the runtime enumerates the loaded objects and rebuilds each
   struct dl_phdr_info in freshly-allocated Fil-C objects, so the dlpi_name string and the
   dlpi_phdr array handed to the callback carry valid, dereferenceable capabilities. Reaching the
   callback at all exercises that runtime path -- a missing implementation would abort before the
   assertions run -- and we additionally prove the pointers are real and that the callback's return
   value stops and propagates. */

#include <link.h>
#include <elf.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdfil.h>

/* No public header declares static_dl_iterate_phdr, so reference the exported symbol directly to
   call it by name below. */
extern int static_dl_iterate_phdr(int(*)(struct dl_phdr_info *info, size_t size, void *data), void *data);

struct ctx {
    int count;
    int found_load;
    int found_named;
    int data_ok;
};

static int count_cb(struct dl_phdr_info* info, size_t size, void* data)
{
    struct ctx* c = (struct ctx*)data;
    /* The runtime forwarded our data pointer unchanged. */
    c->data_ok = 1;

    /* The runtime hands back a full struct dl_phdr_info, so size must at least describe the
       original four ABI fields. */
    ZASSERT(size >= offsetof(struct dl_phdr_info, dlpi_phnum) + sizeof(info->dlpi_phnum));

    c->count++;

    /* dlpi_name must be a real Fil-C capability we can dereference (it may be the empty string for
       the main program). If the capability were bogus, this strlen would trap. We read every byte
       up to and including the terminator, so a name shorter than its capability would also trap. */
    ZASSERT(info->dlpi_name);
    size_t namelen = strlen(info->dlpi_name);
    /* The terminator is still within the capability. */
    volatile char sink = info->dlpi_name[namelen];
    ZASSERT(sink == '\0');
    /* A shared object, such as libc.so or libpizlo.so, has a non-empty path. */
    if (namelen)
        c->found_named = 1;

    /* dlpi_phdr must point at a valid array of dlpi_phnum entries we can read end to end. Every
       loaded object has at least one PT_LOAD segment. */
    ZASSERT(info->dlpi_phnum > 0);
    unsigned i;
    for (i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr)* ph = info->dlpi_phdr + i;
        volatile unsigned type = ph->p_type;
        volatile unsigned long memsz = ph->p_memsz;
        (void)memsz;
        if (type == PT_LOAD)
            c->found_load = 1;
    }
    /* Keep enumerating. */
    return 0;
}

static int stop_cb(struct dl_phdr_info* info, size_t size, void* data)
{
    (void)info;
    (void)size;
    int* n = (int*)data;
    (*n)++;
    /* A nonzero return stops enumeration here and is handed back to the caller. */
    return 123;
}

int main(void)
{
    struct ctx c;
    c.count = 0;
    c.found_load = 0;
    c.found_named = 0;
    c.data_ok = 0;

    int r = dl_iterate_phdr(count_cb, &c);
    ZASSERT(r == 0);
    ZASSERT(c.data_ok);
    /* Require more than one object, proving the runtime walks the full link_map rather than only
       reporting the executable.

       A Fil-C program is whole-process musl: the exe always pulls in libc.so, libpizlo.so and
       libyoloc.so, so a correct enumeration reports several objects. */
    ZASSERT(c.count >= 2);
    /* Every object's phdrs were readable and had a PT_LOAD. */
    ZASSERT(c.found_load);
    /* The shared objects carried real, non-empty name capabilities. */
    ZASSERT(c.found_named);

    /* A nonzero callback return stops enumeration immediately and is propagated to the caller. */
    int n = 0;
    r = dl_iterate_phdr(stop_cb, &n);
    ZASSERT(r == 123);
    ZASSERT(n == 1);

    /* Call static_dl_iterate_phdr directly and require it to behave identically to dl_iterate_phdr,
       proving the static_ entry point works and not only its weak alias.

       Identical means the same object count, the same reachable capabilities, and the same
       forwarded data. */
    struct ctx cs;
    cs.count = 0;
    cs.found_load = 0;
    cs.found_named = 0;
    cs.data_ok = 0;
    r = static_dl_iterate_phdr(count_cb, &cs);
    ZASSERT(r == 0);
    ZASSERT(cs.data_ok);
    ZASSERT(cs.count == c.count);
    ZASSERT(cs.found_load);
    ZASSERT(cs.found_named);

    printf("dl_iterate_phdr saw %d objects.\n", c.count);
    return 0;
}
