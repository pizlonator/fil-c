#
# Check: a unit test framework for C
# Copyright (C) 2011 Mateusz Loskot
# Copyright (C) 2020 Mikko Koivunalho
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
#


set(SOURCES libcompat.c)

set(SOURCES ${SOURCES} fpclassify.c)

if (NOT HAVE_LIBRT)
  set(SOURCES ${SOURCES} clock_gettime.c)
  set(SOURCES ${SOURCES} timer_create.c)
  set(SOURCES ${SOURCES} timer_delete.c)
  set(SOURCES ${SOURCES} timer_settime.c)
endif(NOT HAVE_LIBRT)

if(NOT HAVE_GETLINE)
  set(SOURCES ${SOURCES} getline.c)
endif(NOT HAVE_GETLINE)

if(NOT HAVE_GETTIMEOFDAY)
  set(SOURCES ${SOURCES} gettimeofday.c)
endif(NOT HAVE_GETTIMEOFDAY)

if(NOT HAVE_DECL_LOCALTIME_R)
  set(SOURCES ${SOURCES} localtime_r.c)
endif(NOT HAVE_DECL_LOCALTIME_R)

if(NOT HAVE_MALLOC)
  set(SOURCES ${SOURCES} malloc.c)
endif(NOT HAVE_MALLOC)

if(NOT HAVE_REALLOC)
  set(SOURCES ${SOURCES} realloc.c)
endif(NOT HAVE_REALLOC)

if(NOT HAVE_SNPRINTF)
    set(SOURCES ${SOURCES} snprintf.c)
endif(NOT HAVE_SNPRINTF)

if(NOT HAVE_DECL_STRDUP AND NOT HAVE__STRDUP)
  set(SOURCES ${SOURCES} strdup.c)
endif(NOT HAVE_DECL_STRDUP AND NOT HAVE__STRDUP)

if(NOT HAVE_DECL_STRSIGNAL)
  set(SOURCES ${SOURCES} strsignal.c)
endif(NOT HAVE_DECL_STRSIGNAL)

if(NOT HAVE_DECL_ALARM)
  set(SOURCES ${SOURCES} alarm.c)
endif(NOT HAVE_DECL_ALARM)

if (NOT HAVE_PTHREAD)
  set(SOURCES ${SOURCES} pthread_mutex.c)
endif()

set(HEADERS libcompat.h)

add_library(compat STATIC ${SOURCES} ${HEADERS})

# vim: shiftwidth=2:softtabstop=2:tabstop=2:expandtab:autoindent

