#pragma once

// Copyright transferred from Raider Solutions, Inc to
//   Kern Sibbald and John Walker by express permission.
//
// Copyright (C) 2004-2006 Kern Sibbald
// Copyright (C) 2014 Adam Kropelin
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License as
//   published by the Free Software Foundation; either version 2 of
//   the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
//   General Public License for more details.
//
//   You should have received a copy of the GNU General Public
//   License along with this program; if not, write to the Free
//   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
//   MA 02111-1307, USA.

#ifndef __COMPAT_UNISTD_H_
#define __COMPAT_UNISTD_H_

#include_next <unistd.h>

#define _PC_PATH_MAX 1
#define _PC_NAME_MAX 2

#ifdef __cplusplus
extern "C" {
#endif

long pathconf(const char *, int);
#define getpid _getpid
#define getppid() 0

unsigned int sleep(unsigned int seconds);

#define getuid() 0
#define getgid() 0
#define geteuid() 1
#define getegid() 0

// no-oped sync
__inline void sync(void){};

#ifdef __cplusplus
};
#endif

#endif /* __COMPAT_UNISTD_H_ */