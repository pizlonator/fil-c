/* Quoting for a system command.
   Copyright (C) 2012-2023 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2012.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

/* Specification.  */
#include "system-quote.h"

#include <stdlib.h>
#include <string.h>

#include "sh-quote.h"
#include "xalloc.h"

#if defined _WIN32 && ! defined __CYGWIN__

/* The native Windows CreateProcess() function interprets characters like
   ' ', '\t', '\\', '"' (but not '<' and '>') in a special way:
   - Space and tab are interpreted as delimiters. They are not treated as
     delimiters if they are surrounded by double quotes: "...".
   - Unescaped double quotes are removed from the input. Their only effect is
     that within double quotes, space and tab are treated like normal
     characters.
   - Backslashes not followed by double quotes are not special.
   - But 2*n+1 backslashes followed by a double quote become
     n backslashes followed by a double quote (n >= 0):
       \" -> "
       \\\" -> \"
       \\\\\" -> \\"
   - '*', '?' characters may get expanded through wildcard expansion in the
     callee: By default, in the callee, the initialization code before main()
     takes the result of GetCommandLine(), wildcard-expands it, and passes it
     to main(). The exceptions to this rule are:
       - programs that inspect GetCommandLine() and ignore argv,
       - mingw programs that have a global variable 'int _CRT_glob = 0;',
       - Cygwin programs, when invoked from a Cygwin program.
 */
# define SHELL_SPECIAL_CHARS "\"\\ \001\002\003\004\005\006\007\010\011\012\013\014\015\016\017\020\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037*?"
# define SHELL_SPACE_CHARS " \001\002\003\004\005\006\007\010\011\012\013\014\015\016\017\020\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037"

/* Copies the quoted string to p and returns the number of bytes needed.
   If p is non-NULL, there must be room for system_quote_length (string)
   bytes at p.  */
static size_t
windows_createprocess_quote (char *p, const char *string)
{
  size_t len = strlen (string);
  bool quote_around =
    (len == 0 || strpbrk (string, SHELL_SPECIAL_CHARS) != NULL);
  size_t backslashes = 0;
  size_t i = 0;
# define STORE(c) \
  do                 \
    {                \
      if (p != NULL) \
        p[i] = (c);  \
      i++;           \
    }                \
  while (0)

  if (quote_around)
    STORE ('"');
  for (; len > 0; string++, len--)
    {
      char c = *string;

      if (c == '"')
        {
          size_t j;

          for (j = backslashes + 1; j > 0; j--)
            STORE ('\\');
        }
      STORE (c);
      if (c == '\\')
        backslashes++;
      else
        backslashes = 0;
    }
  if (quote_around)
    {
      size_t j;

      for (j = backslashes; j > 0; j--)
        STORE ('\\');
      STORE ('"');
    }
# undef STORE
  return i;
}

/* The native Windows cmd.exe command interpreter also interprets:
   - '\n', '\r' as a command terminator - no way to escape it,
   - '<', '>' as redirections,
   - '|' as pipe operator,
   - '%var%' as a reference to the environment variable VAR (uppercase),
     even inside quoted strings,
   - '&' '[' ']' '{' '}' '^' '=' ';' '!' '\'' '+' ',' '`' '~' for other
     purposes, according to
     <https://www.microsoft.com/resources/documentation/windows/xp/all/proddocs/en-us/cmd.mspx?mfr=true>
   We quote a string like '%var%' by putting the '%' characters outside of
   double-quotes and the rest of the string inside double-quotes: %"var"%.
   This is guaranteed to not be a reference to an environment variable.
 */
# define CMD_SPECIAL_CHARS "\"\\ \001\002\003\004\005\006\007\010\011\012\013\014\015\016\017\020\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037!%&'*+,;<=>?[]^`{|}~"
# define CMD_FORBIDDEN_CHARS "\n\r"

/* Copies the quoted string to p and returns the number of bytes needed.
   If p is non-NULL, there must be room for system_quote_length (string)
   bytes at p.  */
static size_t
windows_cmd_quote (char *p, const char *string)
{
  size_t len = strlen (string);
  bool quote_around =
    (len == 0 || strpbrk (string, CMD_SPECIAL_CHARS) != NULL);
  size_t backslashes = 0;
  size_t i = 0;
# define STORE(c) \
  do                 \
    {                \
      if (p != NULL) \
        p[i] = (c);  \
      i++;           \
    }                \
  while (0)

  if (quote_around)
    STORE ('"');
  for (; len > 0; string++, len--)
    {
      char c = *string;

      if (c == '"')
        {
          size_t j;

          for (j = backslashes + 1; j > 0; j--)
            STORE ('\\');
        }
      if (c == '%')
        {
          size_t j;

          for (j = backslashes; j > 0; j--)
            STORE ('\\');
          STORE ('"');
        }
      STORE (c);
      if (c == '%')
        STORE ('"');
      if (c == '\\')
        backslashes++;
      else
        backslashes = 0;
    }
  if (quote_around)
    {
      size_t j;

      for (j = backslashes; j > 0; j--)
        STORE ('\\');
      STORE ('"');
    }
  return i;
}

#endif

size_t
system_quote_length (enum system_command_interpreter interpreter,
                     const char *string)
{
  switch (interpreter)
    {
#if !(defined _WIN32 && ! defined __CYGWIN__)
    case SCI_SYSTEM:
#endif
    case SCI_POSIX_SH:
      return shell_quote_length (string);

#if defined _WIN32 && ! defined __CYGWIN__
    case SCI_WINDOWS_CREATEPROCESS:
      return windows_createprocess_quote (NULL, string);

    case SCI_SYSTEM:
    case SCI_WINDOWS_CMD:
      return windows_cmd_quote (NULL, string);
#endif

    default:
      /* Invalid interpreter.  */
      abort ();
    }
}

char *
system_quote_copy (char *p,
                   enum system_command_interpreter interpreter,
                   const char *string)
{
  switch (interpreter)
    {
#if !(defined _WIN32 && ! defined __CYGWIN__)
    case SCI_SYSTEM:
#endif
    case SCI_POSIX_SH:
      return shell_quote_copy (p, string);

#if defined _WIN32 && ! defined __CYGWIN__
    case SCI_WINDOWS_CREATEPROCESS:
      p += windows_createprocess_quote (p, string);
      *p = '\0';
      return p;

    case SCI_SYSTEM:
    case SCI_WINDOWS_CMD:
      p += windows_cmd_quote (p, string);
      *p = '\0';
      return p;
#endif

    default:
      /* Invalid interpreter.  */
      abort ();
    }
}

char *
system_quote (enum system_command_interpreter interpreter,
              const char *string)
{
  switch (interpreter)
    {
#if !(defined _WIN32 && ! defined __CYGWIN__)
    case SCI_SYSTEM:
#endif
    case SCI_POSIX_SH:
      return shell_quote (string);

#if defined _WIN32 && ! defined __CYGWIN__
    case SCI_WINDOWS_CREATEPROCESS:
    case SCI_SYSTEM:
    case SCI_WINDOWS_CMD:
      {
        size_t length = system_quote_length (interpreter, string) + 1;
        char *quoted = XNMALLOC (length, char);
        system_quote_copy (quoted, interpreter, string);
        return quoted;
      }
#endif

    default:
      /* Invalid interpreter.  */
      abort ();
    }
}

char *
system_quote_argv (enum system_command_interpreter interpreter,
                   char * const *argv)
{
  if (*argv != NULL)
    {
      char * const *argp;
      size_t length;
      char *command;
      char *p;

      length = 0;
      for (argp = argv; ; )
        {
          length += system_quote_length (interpreter, *argp) + 1;
          argp++;
          if (*argp == NULL)
            break;
        }

      command = XNMALLOC (length, char);

      p = command;
      for (argp = argv; ; )
        {
          p = system_quote_copy (p, interpreter, *argp);
          argp++;
          if (*argp == NULL)
            break;
          *p++ = ' ';
        }
      *p = '\0';

      return command;
    }
  else
    return xstrdup ("");
}
