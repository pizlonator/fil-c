#
# Check: a unit test framework for C
# Copyright (C) 2011 Mateusz Loskot
# Copyright (C) 2001, 2002 Arien Malec
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

set(SOURCES
  check.c
  check_error.c
  check_list.c
  check_log.c
  check_msg.c
  check_pack.c
  check_print.c
  check_run.c
  check_str.c)

set(HEADERS
  ${CONFIG_HEADER}
  ${CMAKE_CURRENT_BINARY_DIR}/check.h
  check.h.in
  check_error.h
  check_impl.h
  check_list.h
  check_log.h
  check_msg.h
  check_pack.h
  check_print.h
  check_str.h)

configure_file(check.h.in check.h @ONLY)

# To maintain compatibility with the Autotools installation
# we specifically create both shared and static libraries
# as that is what Autotools script has been doing.
# Normally CMake would create the system's native default library type.

add_library(check STATIC ${SOURCES} ${HEADERS})
add_library(Check::check ALIAS check)


# We would like to create an OBJECT library but currently they are
# too unreliable and cumbersome,
# especially with target_link_libraries and install(EXPORT...
# https://stackoverflow.com/questions/38832528/transitive-target-include-directories-on-object-libraries
# So we instead do the work twice.
add_library(checkShared SHARED ${SOURCES} ${HEADERS})
add_library(Check::checkShared ALIAS checkShared)

# Add parts of libcompat as required
target_sources(check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/fpclassify.c)
target_sources(checkShared PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/fpclassify.c)

if (NOT HAVE_LIBRT)
  target_sources(check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/clock_gettime.c)
  target_sources(check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/timer_create.c)
  target_sources(check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/timer_delete.c)
  target_sources(check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/timer_settime.c)
  target_sources(checkShared PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/clock_gettime.c)
  target_sources(checkShared PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/timer_create.c)
  target_sources(checkShared PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/timer_delete.c)
  target_sources(checkShared PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/timer_settime.c)
endif(NOT HAVE_LIBRT)

if(NOT HAVE_GETLINE)
  target_sources(check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/getline.c)
  target_sources(checkShared PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/getline.c)
endif(NOT HAVE_GETLINE)

if(NOT HAVE_GETTIMEOFDAY)
  target_sources(check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/gettimeofday.c)
  target_sources(checkShared PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/gettimeofday.c)
endif(NOT HAVE_GETTIMEOFDAY)

if(NOT HAVE_DECL_LOCALTIME_R)
  target_sources(check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/localtime_r.c)
  target_sources(checkShared PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/localtime_r.c)
endif(NOT HAVE_DECL_LOCALTIME_R)

if(NOT HAVE_MALLOC)
  target_sources(check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/malloc.c)
  target_sources(checkShared PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/malloc.c)
endif(NOT HAVE_MALLOC)

if(NOT HAVE_REALLOC)
  target_sources(check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/realloc.c)
  target_sources(checkShared PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/realloc.c)
endif(NOT HAVE_REALLOC)

if(NOT HAVE_SNPRINTF)
  target_sources(check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/snprintf.c)
  target_sources(checkShared PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/snprintf.c)
endif(NOT HAVE_SNPRINTF)

if(NOT HAVE_DECL_STRDUP AND NOT HAVE__STRDUP)
  target_sources(check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/strdup.c)
  target_sources(checkShared PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/snprintf.c)
endif(NOT HAVE_DECL_STRDUP AND NOT HAVE__STRDUP)

if(NOT HAVE_DECL_STRSIGNAL)
  target_sources(check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/strsignal.c)
  target_sources(checkShared PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/strsignal.c)
endif(NOT HAVE_DECL_STRSIGNAL)

if(NOT HAVE_DECL_ALARM)
  target_sources(check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/alarm.c)
  target_sources(checkShared PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/alarm.c)
endif(NOT HAVE_DECL_ALARM)

if(NOT HAVE_PTHREAD)
  target_sources(check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/pthread_mutex.c)
  target_sources(checkShared PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/pthread_mutex.c)
endif()

# Include libraries if available
if (HAVE_LIBM)
  target_link_libraries(check PUBLIC m)
  target_link_libraries(checkShared PUBLIC m)
endif (HAVE_LIBM)
if (HAVE_LIBRT)
  target_link_libraries(check PUBLIC rt)
  target_link_libraries(checkShared PUBLIC rt)
endif (HAVE_LIBRT)
if (HAVE_SUBUNIT)
  target_link_libraries(check PUBLIC subunit)
  target_link_libraries(checkShared PUBLIC subunit)
endif (HAVE_SUBUNIT)

if(MSVC)
  target_compile_definitions(checkShared
    PRIVATE "CK_DLL_EXP=_declspec(dllexport)"
    INTERFACE "CK_DLL_EXP=_declspec(dllimport)"
  )
endif (MSVC)

# More configuration for exporting

set(LIBRARY_OUTPUT_NAME "check")
list(APPEND public_headers "${CMAKE_CURRENT_BINARY_DIR}/check.h")
list(APPEND public_headers "${CMAKE_CURRENT_BINARY_DIR}/../check_stdint.h")

set_target_properties(check PROPERTIES
  OUTPUT_NAME ${LIBRARY_OUTPUT_NAME}
  PUBLIC_HEADER "${public_headers}"
)

if (MSVC)
  # "On Windows you should probably give each library a different name,
  # since there is a ".lib" file for both shared and static".
  # https://stackoverflow.com/a/2152157/4716395
  # "Dynamic-Link Library" (DLL) is Microsoft terminology.
  # So we call it this:
  set(LIBRARY_OUTPUT_NAME "checkDynamic")
endif (MSVC)
set_target_properties(checkShared PROPERTIES
  OUTPUT_NAME ${LIBRARY_OUTPUT_NAME}
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  PUBLIC_HEADER "${public_headers}"
)
target_include_directories(check
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>
    $<INSTALL_INTERFACE:include>
)
target_include_directories(checkShared
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

if(NOT THIS_IS_SUBPROJECT)
  install(TARGETS check checkShared
    EXPORT check-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
endif()

# vim: shiftwidth=2:softtabstop=2:tabstop=2:expandtab:autoindent

