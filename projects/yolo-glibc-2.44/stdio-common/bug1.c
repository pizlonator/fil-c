#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libc-diag.h>

int
main (void)
{
  char *bp;
  size_t size;
  FILE *stream;
  int lose = 0;

  stream = open_memstream (&bp, &size);
  fprintf (stream, "hello");
  fflush (stream);
  /* clang do not handle %Z format.  */
  DIAG_PUSH_NEEDS_COMMENT_CLANG;
  DIAG_IGNORE_NEEDS_COMMENT_CLANG (13, "-Wformat-invalid-specifier");
  DIAG_IGNORE_NEEDS_COMMENT_CLANG (13, "-Wformat-extra-args");
  printf ("buf = %s, size = %Zu\n", bp, size);
  DIAG_POP_NEEDS_COMMENT_CLANG;
  lose |= size != 5;
  lose |= strncmp (bp, "hello", size);
  fprintf (stream, ", world");
  fclose (stream);
  /* clang do not handle %Z format.  */
  DIAG_PUSH_NEEDS_COMMENT_CLANG;
  DIAG_IGNORE_NEEDS_COMMENT_CLANG (13, "-Wformat-invalid-specifier");
  DIAG_IGNORE_NEEDS_COMMENT_CLANG (13, "-Wformat-extra-args");
  printf ("buf = %s, size = %Zu\n", bp, size);
  DIAG_POP_NEEDS_COMMENT_CLANG;
  lose |= size != 12;
  lose |= strncmp (bp, "hello, world", 12);

  puts (lose ? "Test FAILED!" : "Test succeeded.");

  free (bp);

  return lose;
}
