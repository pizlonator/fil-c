/* Convenience header for conditional use of GNU <libintl.h>.
   Copyright (C) 1995-1998, 2000-2002 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU Library General Public License as published
   by the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA.  */

// NOTE: This file MUST be the last file included to avoid problems
//       with system header files that might include libintl.h

#ifndef _LIBGETTEXT_H
#define _LIBGETTEXT_H 1

/* NLS can be disabled through the configure --disable-nls option.  */
#if ENABLE_NLS

/* Get declarations of GNU message catalog functions.  */
# include <libintl.h>

#else

/* Solaris /usr/include/locale.h includes /usr/include/libintl.h, which
   chokes if dcgettext is defined as a macro.  So include it now, to make
   later inclusions of <locale.h> a NOP.  We don't include <libintl.h>
   as well because people using "gettext.h" will not include <libintl.h>,
   and also including <libintl.h> would fail on SunOS 4, whereas <locale.h>
   is OK.  */
#if defined(__sun)
# include <locale.h>
#endif

/* Disabled NLS.
   The casts to 'const char *' serve the purpose of producing warnings
   for invalid uses of the value returned from these functions.
   On pre-ANSI systems without 'const', the config.h file is supposed to
   contain "#define const".  */
# undef  gettext
# define gettext(Msgid) ((const char *) (Msgid))
# undef  dgettext
# define dgettext(Domainname, Msgid) ((const char *) (Msgid))
# undef  dcgettext
# define dcgettext(Domainname, Msgid, Category) ((const char *) (Msgid))
# undef  ngettext
# define ngettext(Msgid1, Msgid2, N) \
    ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
# undef  dngettext
# define dngettext(Domainname, Msgid1, Msgid2, N) \
    ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
# undef  dcngettext
# define dcngettext(Domainname, Msgid1, Msgid2, N, Category) \
    ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
# undef  textdomain
# define textdomain(Domainname) ((const char *) (Domainname))
# undef  bindtextdomain
# define bindtextdomain(Domainname, Dirname) ((const char *) (Dirname))
# undef  bind_textdomain_codeset
# define bind_textdomain_codeset(Domainname, Codeset) ((const char *) (Codeset))

#endif

/* A pseudo function call that serves as a marker for the automated
   extraction of messages, but does not call gettext().  The run-time
   translation is done at a different place in the code.
   The argument, String, should be a literal string.  Concatenated strings
   and other string expressions won't work.
   The macro's expansion is not parenthesized, so that it is suitable as
   initializer for static 'char[]' or 'const char[]' variables.  */
#define gettext_noop(String) String

/* short cut macros */

/* I use dgettext so that the right domain will be looked at when
   Aspell is used as a library */
#define _(String) dgettext ("aspell", String)
#define N_(String) gettext_noop (String)

/* use gt_ when there is the possibility that str will be the empty
   string.  gettext in this case is not guaranteed to return an
   empty string */
static inline const char * gt_(const char * str) {
  return str[0] == '\0' ? str : _(str);
}

extern "C" void aspell_gettext_init();

/* NOTE: DO NOT USE "gettext", ALWAYS USE "_" BECAUSE WHEN ASPELL IS USED
   AS A LIBRARY THE DOMAIN IS NOT GUARANTEED TO BE ASPELL */

#endif /* _LIBGETTEXT_H */
