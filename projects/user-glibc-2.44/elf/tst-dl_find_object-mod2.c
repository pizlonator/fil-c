#include <dlfcn.h>

char mod2_data;

void
mod2_function (void (*f) (void))
{
  /* Make sure this is not a tail call and unwind information is
     therefore needed.  */
  f ();
  f ();
}

/* Used to verify that _dl_find_object after static dlopen works.  */
void *find_object = _dl_find_object;
