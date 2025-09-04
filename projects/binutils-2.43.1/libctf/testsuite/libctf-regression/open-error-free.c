/* Make sure that, on error, an opened dict is properly freed.  */

#define _GNU_SOURCE 1
#include "config.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctf-api.h>
#include <ctf.h>

#ifdef HAVE_VALGRIND_VALGRIND_H
#include <valgrind/valgrind.h>
#endif

static unsigned long long malloc_count;
static unsigned long long free_count;

static void *(*real_malloc) (size_t size);
static void (*real_free) (void *ptr);
static void *(*real_realloc) (void *ptr, size_t size);
static void *(*real_calloc) (size_t nmemb, size_t size);

/* Interpose malloc/free functionss and count calls to spot unbalanced ones.
   Extra complexity to deal with dlsym() calling dlerror() and thus calloc() --
   luckily it handles malloc failure fine, so we can just always fail before the
   hooks are installed.  */

static int in_hooks;

static void hook_init (void)
{
  if (!real_calloc)
    {
      in_hooks = 1;
      real_calloc = (void *(*) (size_t, size_t)) dlsym (RTLD_NEXT, "calloc");
      real_malloc = (void *(*) (size_t)) dlsym (RTLD_NEXT, "malloc");
      real_free = (void (*) (void *)) dlsym (RTLD_NEXT, "free");
      real_realloc = (void *(*) (void *, size_t)) dlsym (RTLD_NEXT, "realloc");
      if (!real_malloc || !real_free || !real_realloc || !real_calloc)
	{
	  fprintf (stderr, "Cannot hook malloc\n");
	  exit(1);
	}
      in_hooks = 0;
    }
}

void *malloc (size_t size)
{
  if (in_hooks)
    return NULL;

  hook_init();
  malloc_count++;
  return real_malloc (size);
}

void *realloc (void *ptr, size_t size)
{
  void *new_ptr;

  if (in_hooks)
    return NULL;

  hook_init();
  new_ptr = real_realloc (ptr, size);

  if (!ptr)
    malloc_count++;

  if (size == 0)
    free_count++;

  return new_ptr;
}

void *calloc (size_t nmemb, size_t size)
{
  void *ptr;

  if (in_hooks)
    return NULL;

  hook_init();
  ptr = real_calloc (nmemb, size);

  if (ptr)
    malloc_count++;
  return ptr;
}

void free (void *ptr)
{
  hook_init();

  if (in_hooks)
    return;

  if (ptr != NULL)
    free_count++;

  return real_free (ptr);
}

int main (void)
{
  ctf_dict_t *fp;
  ctf_encoding_t e = { CTF_INT_SIGNED, 0, sizeof (long) };
  int err;
  ctf_id_t type;
  size_t i;
  char *written;
  size_t written_size;
  char *foo;
  ctf_next_t *it = NULL;
  unsigned long long frozen_malloc_count, frozen_free_count;

#ifdef HAVE_VALGRIND_VALGRIND_H
  if (RUNNING_ON_VALGRIND)
    {
      printf ("UNSUPPORTED: valgrind interferes with malloc counting\n");
      return 0;
    }
#endif

  if ((fp = ctf_create (&err)) == NULL)
    goto open_err;

  /* Define an integer, then a pile of unconnected pointers to it, just to
     use up space..  */

  if ((type = ctf_add_integer (fp, CTF_ADD_ROOT, "long", &e)) == CTF_ERR)
    goto err;

  for (i = 0; i < 100; i++)
    {
      if (ctf_add_pointer (fp, CTF_ADD_ROOT, type) == CTF_ERR)
	goto err;
    }

  /* Write the dict out, uncompressed (to stop it failing to open due to decompression
     failure after we corrupt it: the leak is only observable if the dict gets
     malloced, which only happens after that point.)  */

  if ((written = (char *) ctf_write_mem (fp, &written_size, (size_t) -1)) == NULL)
    goto write_err;

  ctf_dict_close (fp);

  /* Corrupt the dict.  */

  memset (written + sizeof (ctf_header_t), 64, 64);

  /* Reset the counters: we are interested only in leaks at open
     time.  */
  malloc_count = 0;
  free_count = 0;

  if ((ctf_simple_open (written, written_size, NULL, 0, 0, NULL, 0, &err)) != NULL)
    {
      fprintf (stderr, "wildly corrupted dict still opened OK?!\n");
      exit (1);
    }

  /* The error log will have accumulated errors which need to be
     consumed and freed if they are not to appear as a spurious leak.  */

  while ((foo = ctf_errwarning_next (NULL, &it, NULL, NULL)) != NULL)
    free (foo);

  frozen_malloc_count = malloc_count;
  frozen_free_count = free_count;

  if (frozen_malloc_count == 0)
    fprintf (stderr, "Allocation count after failed open is zero: likely hook failure.\n");
  else if (frozen_malloc_count > frozen_free_count)
    fprintf (stderr, "Memory leak is present: %lli allocations (%lli allocations, %lli frees).\n",
	     frozen_malloc_count - frozen_free_count, frozen_malloc_count, frozen_free_count);
  else if (frozen_malloc_count < frozen_free_count)
    fprintf (stderr, "Possible double-free: %lli allocations, %lli frees.\n",
	     frozen_malloc_count, frozen_free_count);

  printf ("All OK.\n");
  exit (0);

 open_err:
  fprintf (stderr, "Cannot open/create: %s\n", ctf_errmsg (err));
  exit (1);

 err:
  fprintf (stderr, "Cannot add: %s\n", ctf_errmsg (ctf_errno (fp)));
  exit (1);

 write_err:
  fprintf (stderr, "Cannot write: %s\n", ctf_errmsg (ctf_errno (fp)));
  exit (1);
}
