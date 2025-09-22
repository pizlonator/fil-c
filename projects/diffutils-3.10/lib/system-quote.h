/* Quoting for a system command.
   Copyright (C) 2001-2023 Free Software Foundation, Inc.
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

#ifndef _SYSTEM_QUOTE_H
#define _SYSTEM_QUOTE_H

/* When passing a command the system's command interpreter, we must quote the
   program name and arguments, since
     - Unix shells interpret characters like " ", "'", "<", ">", "$", '*', '?'
       etc. in a special way,
     - Windows CreateProcess() interprets characters like ' ', '\t', '\\', '"'
       etc. (but not '<' and '>') in a special way,
     - Windows cmd.exe also interprets characters like '<', '>', '&', '%', etc.
       in a special way.  Note that it is impossible to pass arguments that
       contain newlines or carriage return characters to programs through
       cmd.exe.
     - Windows programs usually perform wildcard expansion when they receive
       arguments that contain unquoted '*', '?' characters.

  With this module, you can build a command that will invoke a program with
  specific strings as arguments.

  Note: If you want wildcard expansion to happen, you have to first do wildcard
  expansion through the 'glob' module, then quote the resulting strings through
  this module, and then invoke the system's command interpreter.

  Limitations:
    - When invoking native Windows programs on Windows Vista or newer,
      wildcard expansion will occur in the invoked program nevertheless.
    - On native Windows, for SCI_SYSTEM and SCI_WINDOWS_CMD, newlines and
      carriage return characters are not supported.  Their undesired effect
      is to truncate the entire command line.
 */

/* This file uses _GL_ATTRIBUTE_MALLOC, _GL_ATTRIBUTE_RETURNS_NONNULL.  */
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Identifier for the kind of interpreter of the command.  */
enum system_command_interpreter
{
  /* The interpreter used by the system() and popen() functions.
     This is equivalent to SCI_POSIX_SH on Unix platforms and
     SCI_WINDOWS_CMD on native Windows platforms.  */
  SCI_SYSTEM                    = 0
  /* The POSIX /bin/sh.  */
  , SCI_POSIX_SH                = 1
#if defined _WIN32 && ! defined __CYGWIN__
  /* The native Windows CreateProcess() function.  */
  , SCI_WINDOWS_CREATEPROCESS   = 2
  /* The native Windows cmd.exe interpreter.  */
  , SCI_WINDOWS_CMD             = 3
#endif
};

/* Returns the number of bytes needed for the quoted string.  */
extern size_t
       system_quote_length (enum system_command_interpreter interpreter,
                            const char *string);

/* Copies the quoted string to p and returns the incremented p.
   There must be room for system_quote_length (string) + 1 bytes at p.  */
extern char *
       system_quote_copy (char *restrict p,
                          enum system_command_interpreter interpreter,
                          const char *string);

/* Returns the freshly allocated quoted string.  */
extern char *
       system_quote (enum system_command_interpreter interpreter,
                     const char *string)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_RETURNS_NONNULL;

/* Returns a freshly allocated string containing all argument strings, quoted,
   separated through spaces.  */
extern char *
       system_quote_argv (enum system_command_interpreter interpreter,
                          char * const *argv)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_RETURNS_NONNULL;

#ifdef __cplusplus
}
#endif

#endif /* _SYSTEM_QUOTE_H */
