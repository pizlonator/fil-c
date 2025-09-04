#include <stdio.h>

int
main (void)
{
  char buf[80];
  int i;
  int lost = 0;

  lost = (scanf ("%2s", buf) < 0);
  lost |= (buf[0] != 'X' || buf[1] != 'Y' || buf[2] != '\0');
  if (lost)
    puts ("test of %2s failed.");
  lost |= (scanf (" ") < 0);
  lost |= (scanf ("%d", &i) < 0);
  lost |= (i != 1234);
  if (lost)
    puts ("test of %d failed.");
  lost |= (scanf ("%c", buf) < 0);
  lost |= (buf[0] != 'L');
  if (lost)
    puts ("test of %c failed.\n");

  puts (lost ? "Test FAILED!" : "Test succeeded.");
  return lost;
}
