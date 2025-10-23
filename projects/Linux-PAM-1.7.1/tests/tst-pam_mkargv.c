/*
   Copyright (C) Thorsten Kukuk <kukuk@suse.de> 2009

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation in version 2 of the License.
*/

#include <config.h>

#include <stdio.h>
#include <string.h>

#include "pam_misc.c"

/* Simple program to see if _pam_mkargv() would succeed. */
int main(void)
{
  static const char argvstring[] = "user = XENDT\\userα user=XENDT\\user1";
  static const char * const argvresult[] = {"user", "=", "XENDT\\userα",
                                            "user=XENDT\\user1"};
  int myargc;
  char **myargv;
  int argvlen;
  int explen;
  int i;

  explen = sizeof(argvstring) * ((sizeof(char)) + sizeof(char *));
  argvlen = _pam_mkargv(argvstring, &myargv, &myargc);

#if 0
  printf ("argvlen=%i, argc=%i", argvlen, myargc);
  for (i = 0; i < myargc; i++) {
    printf(", argv[%d]=%s", i, myargv[i]);
  }
  printf ("\n");
#endif

  if (argvlen != explen)
    return 1;

  if (myargc != 4)
    return 1;

  for (i = 0; i < 4; i++)
    {
      if (strcmp (myargv[i], argvresult[i]) != 0)
	return 1;
    }

  free(myargv);

  return 0;
}
