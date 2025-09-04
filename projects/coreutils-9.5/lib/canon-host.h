/* Host name canonicalization

   Copyright (C) 2005, 2009-2024 Free Software Foundation, Inc.

   Written by Derek Price <derek@ximbiot.com>

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#ifndef CANON_HOST_H
# define CANON_HOST_H 1

/* This file uses _GL_ATTRIBUTE_MALLOC.  */
# if !_GL_CONFIG_H_INCLUDED
#  error "Please include config.h first."
# endif

# include <stdlib.h>

char *canon_host (char const *host)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE;
char *canon_host_r (char const *host, int *cherror)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE;

const char *ch_strerror (void);
# define ch_strerror_r(cherror) gai_strerror (cherror);

#endif /* !CANON_HOST_H */
