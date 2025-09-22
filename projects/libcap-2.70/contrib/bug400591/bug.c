#include <assert.h>
#include <stdlib.h>
#include <sys/capability.h>

/*
 * Original from http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=400591
 *
 * Modified to test more functions.. AGM - 2008/07/06.
 */

int main (int argc, char *argv[])
{
  cap_t caps, caps2;
  ssize_t size, copy_size;
  void *buffer;
  char *text1, *text2;

  assert((caps = cap_get_pid(1)));

  text1 = cap_to_text(caps, NULL);
  assert(text1);

  size = cap_size (caps);
  assert (size>0  && size<1024);

  buffer = malloc (size);
  assert (buffer);

  copy_size = cap_copy_ext (buffer, caps, size);
  assert (copy_size == size);

  caps2 = cap_copy_int(buffer);
  assert (caps2);
  
  text2 = cap_to_text(caps2, NULL);
  assert(text2);

  assert(strcmp(text1, text2) == 0);

  assert(cap_compare(caps, caps2) == 0);

  return 0;
}
