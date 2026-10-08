#include <elf.h>
#include <link.h>
#include "pthread_impl.h"
#include "libc.h"
#include <pizlonated_syscalls.h>

/* The Fil-C runtime enumerates the loaded objects (via the host dl_iterate_phdr) and builds, for
   each one, a `struct dl_phdr_info` whose `dlpi_name` and `dlpi_phdr` pointers carry valid Fil-C
   capabilities before invoking `callback`. Doing this in the runtime is required: the phdrs live in
   the host's ELF image, which user (Fil-C) code cannot legally form a pointer to.

   Exported under its own name too, not only through the dl_iterate_phdr weak alias below. */
int static_dl_iterate_phdr(int(*callback)(struct dl_phdr_info *info, size_t size, void *data), void *data)
{
	return zsys_dl_iterate_phdr((void *)callback, data);
}

weak_alias(static_dl_iterate_phdr, dl_iterate_phdr);
