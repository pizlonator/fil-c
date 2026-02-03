/*
 * Copyright (c) 2000-2004
 * Kevin Atkinson
 * Jose Da Silva
 *
 * Word-list-compress Version 0.2.1.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies  
 * and that both that copyright notice and this permission notice 
 * appear in supporting documentation.  Kevin Atkinson makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * Bug fixes and enhancements by Jose Da Silva, 2004.
 *
 */

#include <stdio.h>

#if defined(__CYGWIN__) || defined (_WIN32)

#  include <io.h>
#  include <fcntl.h>

#  define SETBIN(fno)  _setmode( _fileno( fno ), _O_BINARY )

#else

#  define SETBIN(fno)

#endif

#define BUFSIZE 256	/* BUFSIZE must be 256 */

static void usage ()
{
  fputs("Compresses or uncompresses sorted word lists.  Version 0.2.1\n",       stderr);
  fputs("For best result the locale should be set to C before sorting by\n",    stderr);
  fputs("  setting the environmental variable LANG to \"C\" before sorting.\n", stderr);
  fputs("Copyright 2000-2004 by Kevin Atkinson.\n",                             stderr);
  fputs("Usage: word-list-compress c[ompress]|d[ecompress]\n",       stderr);
}

/* PRECOND: bufsize >= 2 */
static int get_word(FILE * in, char * w)
{
  int bufsize = BUFSIZE - 1;
  register int c;

  while (c = getc(in), c <= 32 && c != EOF);
  if (c != EOF) {
    do {
      *w++ = (char)(c);
    } while (c = getc(in), c > 32 && c != EOF && --bufsize);
  }
  *w = '\0';
  ungetc(c, in);
  if (c == EOF) return 0; /* done */
  if (bufsize)  return 1; /* normal return */
  return 2;		  /* error, word larger than 255 chars */
}

int main (int argc, const char *argv[]) {

  if (argc == 2) {
    char c = argv[1][0];
    if (c == '-') c = argv[1][1];

    if (c == 'c') {

      char s1[BUFSIZE];
      char s2[BUFSIZE];
      char * prev = s2;
      char * cur = s1;
      int errFlag;
      *prev = '\0';

      SETBIN (stdout);

      while ((errFlag = get_word(stdin, cur)) == 1) {
        int i = 0;
        /* get the length of the prefix */
	while (prev[i] != '\0' && prev[i] == cur[i])
          ++i;
        if (i > 31) {
	  if (putc('\0', stdout) < 0) goto error_out_c;
        }
	if (putc(i+1, stdout) < 0) goto error_out_c;
	if (fputs(cur+i, stdout) < 0) goto error_out_c;

	/* swap prev and next */
	{
	  char * tmp = cur;
	  cur = prev;
	  prev = tmp;
        }
      }
      if (fflush(stdout) < 0) goto error_out_c;
      if (errFlag) goto error_in_c;
      return 0;
    }

    if (c == 'd') {
    
      char cur[BUFSIZE+1];
      int i;
      int c;
      int last_max = 0;

      SETBIN (stdin);
    
      i = getc(stdin);
      if (i != 1) goto error_in_d;
      while (i != -1) {
        if (i == 0)
          i = getc(stdin);
        --i;
        if (i < 0 || i > last_max) goto error_in_d;
        while ((c = getc(stdin)) > 32 && i < BUFSIZE)
          cur[i++] = (char)c;
	if (i >= BUFSIZE) goto error_in_d;
        last_max = i;
	cur[i] = '\n'; cur[++i] = '\0';
	if (fputs(cur, stdout) < 0) goto error_out_d;
        i = c;
      }
      return 0;
      
    error_in_c:
    error_in_d:
      fputs("ERROR: Corrupt Input.\n", stderr);
      return 2;

    error_out_c:
    error_out_d:
      /* output space full or other output fault */
      fputs("ERROR: Output Data Error.\n", stderr);
      return 3;
    }
  }
  
  usage();
  return 1;
}
