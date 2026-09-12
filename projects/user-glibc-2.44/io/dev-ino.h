/* A simple (device, inode) struct.
   Copyright (C) 2003-2026 Free Software Foundation, Inc.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Jim Meyering, 2003.  */

#ifndef DEV_INO_H
#define DEV_INO_H 1

#include <sys/types.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef INO_T
#define INO_T ino_t
#endif

struct dev_ino
{
  INO_T st_ino;
  dev_t st_dev;
};


#ifdef __cplusplus
}
#endif

#endif
