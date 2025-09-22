/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */

/* Declarations for wide characters */
/* This file must be included last because the redefinition of wchar_t may
   cause conflicts when system include files were included after it. */

#ifdef HAVE_WIDECHAR

# include <wchar.h>
# include <wctype.h>

#else /* !HAVE_WIDECHAR */

# include <ctype.h>
  /* Fallback for types */
# define wchar_t char
# define wint_t int
# ifndef WEOF
#  define WEOF EOF
# endif

 /* Fallback for input operations */
# define fgetwc fgetc
# define getwc getc
# define getwchar getchar
# define fgetws fgets

  /* Fallback for output operations */
# define fputwc fputc
# define putwc putc
# define putwchar putchar
# define fputws fputs

  /* Fallback for character classification */
# define iswgraph isgraph
# define iswprint isprint
# define iswspace isspace

  /* Fallback for string functions */
# define wcschr strchr
# define wcsdup strdup
# define wcslen strlen
# define wcspbrk strpbrk

# define wcwidth(c) (1)
# define wmemset memset
# define ungetwc ungetc

#endif /* HAVE_WIDECHAR */
