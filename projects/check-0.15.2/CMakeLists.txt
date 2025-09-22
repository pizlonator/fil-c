#
# Check: a unit test framework for C
#
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
cmake_minimum_required(VERSION 3.9 FATAL_ERROR)

include(CheckCCompilerFlag)

# Detect if Check is being used in another build as a subproject
# probably with command FetchContent*().
set(THIS_IS_SUBPROJECT FALSE)
if(DEFINED PROJECT_NAME)
  set(THIS_IS_SUBPROJECT TRUE)
  message(STATUS "Turned off installing because Check is a subproject.")
  message(STATUS "Turned off building tests because Check is a subproject.")
endif()

if(POLICY CMP0090)
  # export(PACKAGE) does not populate package registry by default. (NEW)
  cmake_policy(SET CMP0090 NEW)
endif()
if(POLICY CMP0076)
  # target_sources() leaves relative source file paths unmodified. (OLD)
  cmake_policy(SET CMP0076 OLD)
endif()
project(Check
  DESCRIPTION "Unit Testing Framework for C"
  LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

###############################################################################
# Set build features

# Set CMAKE_BUILD_TYPE to Debug if source directory is a Git repository
# or user does not override on the command line
include(BuildType)

###############################################################################
# Configure a project for testing with CTest/CDash
include(CTest)

macro(extract_version file setting_name)
  file(STRINGS ${file} VERSION_NUMBER REGEX "^${setting_name}")
  string(REPLACE "=" ";" VERSION_NUMBER_LIST ${VERSION_NUMBER})
  list(GET VERSION_NUMBER_LIST 1 ${setting_name})
endmacro(extract_version)

extract_version(configure.ac CHECK_MAJOR_VERSION)
extract_version(configure.ac CHECK_MINOR_VERSION)
extract_version(configure.ac CHECK_MICRO_VERSION)

set(PROJECT_VERSION_MAJOR ${CHECK_MAJOR_VERSION})
set(PROJECT_VERSION_MINOR ${CHECK_MINOR_VERSION})
set(PROJECT_VERSION_PATCH ${CHECK_MICRO_VERSION})
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

###############################################################################
# Provides install directory variables as defined by the GNU Coding Standards.
include(GNUInstallDirs)

###############################################################################
# Follow ISO C99 standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)          # Use GNU extensions and POSIX standard

###############################################################################
# Option
option(CHECK_ENABLE_TESTS
  "Deprecated: Enable the compilation and running of Check's unit tests" ON)
if(NOT CHECK_ENABLE_TESTS)
  message(DEPRECATION "The option CHECK_ENABLE_TESTS is deprecated. Use option BUILD_TESTING.")
  # TODO Remove this option by Check 0.15.0!
endif(NOT CHECK_ENABLE_TESTS)

option(CHECK_ENABLE_GCOV
  "Turn on test coverage" OFF)
if (CHECK_ENABLE_GCOV AND NOT ${CMAKE_C_COMPILER_ID} MATCHES "GNU")
  message(FATAL_ERROR "Code Coverage (gcov) only works if GNU compiler is used!")
endif (CHECK_ENABLE_GCOV AND NOT ${CMAKE_C_COMPILER_ID} MATCHES "GNU")

option(ENABLE_MEMORY_LEAKING_TESTS
  "Enable certain memory leaking tests only if valgrind is not used in testing" ON)

option(CHECK_ENABLE_TIMEOUT_TESTS
  "Enable Check's timeout related unit tests" ON)
if(CHECK_ENABLE_TIMEOUT_TESTS)
  add_definitions(-DTIMEOUT_TESTS_ENABLED=1)
else(CHECK_ENABLE_TIMEOUT_TESTS)
  add_definitions(-DTIMEOUT_TESTS_ENABLED=0)
endif(CHECK_ENABLE_TIMEOUT_TESTS)
###############################################################################
# Check system and architecture
if(WIN32)
  if(MSVC60)
  set(WINVER 0x0400)
  else()
  set(WINVER 0x0600)
  endif()
  set(_WIN32_WINNT ${WINVER})
endif(WIN32)

if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_definitions(-D_CRT_NONSTDC_NO_WARNINGS)
endif(MSVC)

###############################################################################
include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckStructMember)
include(CheckSymbolExists)
include(CheckTypeExists)
include(CheckTypeSize)

###############################################################################
# Check headers
set(INCLUDES "")
macro(ck_check_include_file header var)
  check_include_files("${INCLUDES};${header}" ${var})
  if(${var})
    set(INCLUDES ${INCLUDES} ${header})
  endif(${var})
endmacro(ck_check_include_file)

# Some FreeBSD headers assume sys/types.h was already included.
ck_check_include_file("sys/types.h" HAVE_SYS_TYPES_H)

# Alphabetize the rest unless there's a compelling reason
ck_check_include_file("errno.h" HAVE_ERRNO_H)
ck_check_include_file("inttypes.h" HAVE_INTTYPES_H)
ck_check_include_file("limits.h" HAVE_LIMITS_H)
ck_check_include_file("regex.h" HAVE_REGEX_H)
ck_check_include_file("signal.h" HAVE_SIGNAL_H)
ck_check_include_file("stdarg.h" HAVE_STDARG_H)
ck_check_include_file("stdint.h" HAVE_STDINT_H)
ck_check_include_file("stdlib.h" HAVE_STDLIB_H)
ck_check_include_file("string.h" HAVE_STRING_H)
ck_check_include_file("strings.h" HAVE_STRINGS_H)
ck_check_include_file("sys/time.h" HAVE_SYS_TIME_H)
ck_check_include_file("time.h" HAVE_TIME_H)
ck_check_include_file("unistd.h" HAVE_UNISTD_H)
ck_check_include_file("pthread.h" HAVE_PTHREAD)

# check if we have windows.h on native windows environments
ck_check_include_file("windows.h" HAVE_WINDOWS_H)

###############################################################################
# Check functions
check_function_exists(fork HAVE_FORK)
check_function_exists(getline HAVE_GETLINE)
check_function_exists(getpid HAVE_GETPID)
check_function_exists(gettimeofday HAVE_GETTIMEOFDAY)
check_function_exists(localtime_r HAVE_DECL_LOCALTIME_R)
check_function_exists(malloc HAVE_MALLOC)
check_function_exists(mkstemp HAVE_MKSTEMP)
check_function_exists(realloc HAVE_REALLOC)
check_function_exists(setenv HAVE_DECL_SETENV)
check_function_exists(sigaction HAVE_SIGACTION)
check_function_exists(strdup HAVE_DECL_STRDUP)
check_function_exists(strsignal HAVE_DECL_STRSIGNAL)
check_function_exists(_getpid HAVE__GETPID)
check_function_exists(_strdup HAVE__STRDUP)
check_function_exists(alarm HAVE_DECL_ALARM)
if (HAVE_WINDOWS_H)
  check_function_exists(InitOnceBeginInitialize HAVE_INIT_ONCE_BEGIN_INITIALIZE)
  check_function_exists(InitOnceComplete HAVE_INIT_ONCE_COMPLETE)
endif()
if (HAVE_REGEX_H)
  check_function_exists(regcomp HAVE_REGCOMP)
  check_function_exists(regexec HAVE_REGEXEC)
endif()

# printf related checks
check_function_exists(snprintf HAVE_SNPRINTF_FUNCTION)
check_function_exists(vsnprintf HAVE_VSNPRINTF_FUNCTION)
check_symbol_exists(snprintf stdio.h HAVE_SNPRINTF_SYMBOL)
check_symbol_exists(vsnprintf stdio.h HAVE_VSNPRINTF_SYMBOL)

if(NOT HAVE_SNPRINTF_FUNCTION AND NOT HAVE_SNPRINTF_SYMBOL)
  add_definitions(-Dsnprintf=rpl_snprintf)
  set(snprintf rpl_snprintf)
  add_definitions(-Dvsnprintf=rpl_vsnprintf)
  set(vsnprintf rpl_vsnprintf)
else(NOT HAVE_SNPRINTF_FUNCTION AND NOT HAVE_SNPRINTF_SYMBOL)
  set(HAVE_SNPRINTF 1)
  add_definitions(-DHAVE_SNPRINTF=1)
  set(HAVE_VSNPRINTF 1)
  add_definitions(-DHAVE_VSNPRINTF=1)
endif(NOT HAVE_SNPRINTF_FUNCTION AND NOT HAVE_SNPRINTF_SYMBOL)

if(HAVE_FORK)
  add_definitions(-DHAVE_FORK=1)
  set(HAVE_FORK 1)
else(HAVE_FORK)
  add_definitions(-DHAVE_FORK=0)
  set(HAVE_FORK 0)
endif(HAVE_FORK)

if(HAVE_MKSTEMP)
  add_definitions(-DHAVE_MKSTEMP=1)
  set(HAVE_MKSTEMP 1)
else(HAVE_MKSTEMP)
  add_definitions(-DHAVE_MKSTEMP=0)
  set(HAVE_MKSTEMP 0)
endif(HAVE_MKSTEMP)

if(HAVE_DECL_ALARM)
  add_definitions(-DHAVE_DECL_ALARM=1)
  set(HAVE_DECL_ALARM 1)
else(HAVE_DECL_ALARM)
  add_definitions(-DHAVE_DECL_ALARM=0)
  set(HAVE_DECL_ALARM 0)
endif(HAVE_DECL_ALARM)

if(HAVE_REGEX_H AND HAVE_REGCOMP AND HAVE_REGEXEC)
  add_definitions(-DHAVE_REGEX=1)
  set(HAVE_REGEX 1)
  add_definitions(-DENABLE_REGEX=1)
  set(ENABLE_REGEX 1)
endif()

if (HAVE_PTHREAD)
  check_c_compiler_flag("-pthread" HAVE_PTHREADS_FLAG)
  if (HAVE_PTHREADS_FLAG)
    add_definitions("-pthread")
    add_link_options("-pthread")
  endif()
endif()


###############################################################################
# Check defines
set(headers "limits.h")

if(HAVE_STDINT_H)
  list(APPEND headers "stdint.h")
endif(HAVE_STDINT_H)

if(HAVE_INTTYPES_H)
  list(APPEND headers "inttypes.h")
endif(HAVE_INTTYPES_H)

check_symbol_exists(INT64_MAX "${headers}" HAVE_INT64_MAX)
check_symbol_exists(INT64_MIN "${headers}" HAVE_INT64_MIN)
check_symbol_exists(UINT32_MAX "${headers}" HAVE_UINT32_MAX)
check_symbol_exists(UINT64_MAX "${headers}" HAVE_UINT64_MAX)
check_symbol_exists(SIZE_MAX "${headers}" HAVE_SIZE_MAX)
check_symbol_exists(SSIZE_MAX "limits.h" HAVE_SSIZE_MAX)

###############################################################################
# Check struct members

# Check for  tv_sec in struct timeval
if(NOT HAVE_SYS_TIME_H)
  if(MSVC)
    check_struct_member("struct timeval" tv_sec "Winsock2.h" HAVE_STRUCT_TIMEVAL_TV_SEC)
    check_struct_member("struct timeval" tv_usec "Winsock2.h" HAVE_STRUCT_TIMEVAL_TV_USEC)
    check_struct_member("struct timespec" tv_sec "Winsock2.h" HAVE_WINSOCK2_H_STRUCT_TIMESPEC_TV_SEC)
    check_struct_member("struct timespec" tv_sec "time.h" HAVE_TIME_H_STRUCT_TIMESPEC_TV_SEC)
    check_struct_member("struct itimerspec" it_value "Winsock2.h" HAVE_STRUCT_ITIMERSPEC_IT_VALUE)

    if(NOT HAVE_WINSOCK2_H_STRUCT_TIMESPEC_TV_SEC AND NOT HAVE_TIME_H_STRUCT_TIMESPEC_TV_SEC)
      add_definitions(-DSTRUCT_TIMESPEC_DEFINITION_MISSING=1)
      set(STRUCT_TIMESPEC_DEFINITION_MISSING 1)
    endif(NOT HAVE_WINSOCK2_H_STRUCT_TIMESPEC_TV_SEC AND NOT HAVE_TIME_H_STRUCT_TIMESPEC_TV_SEC)

    if(NOT HAVE_STRUCT_ITIMERSPEC_IT_VALUE)
      add_definitions(-DSTRUCT_ITIMERSPEC_DEFINITION_MISSING=1)
      set(STRUCT_ITIMERSPEC_DEFINITION_MISSING 1)
    endif(NOT HAVE_STRUCT_ITIMERSPEC_IT_VALUE)
  endif(MSVC)
endif(NOT HAVE_SYS_TIME_H)

# OSX has sys/time.h, but it still lacks itimerspec
if(HAVE_SYS_TIME_H)
  check_struct_member("struct itimerspec" it_value "sys/time.h" HAVE_STRUCT_ITIMERSPEC_IT_VALUE)
  if(NOT HAVE_STRUCT_ITIMERSPEC_IT_VALUE)
    add_definitions(-DSTRUCT_ITIMERSPEC_DEFINITION_MISSING=1)
    set(STRUCT_ITIMERSPEC_DEFINITION_MISSING 1)
  endif(NOT HAVE_STRUCT_ITIMERSPEC_IT_VALUE)
endif(HAVE_SYS_TIME_H)

###############################################################################
# Check for integer types
check_type_size("short" SIZE_OF_SHORT)
check_type_size("int" SIZE_OF_INT)
check_type_size("long" SIZE_OF_LONG)
check_type_size("long long" SIZE_OF_LONG_LONG)

check_type_size("unsigned short" SIZE_OF_UNSIGNED_SHORT)
check_type_size("unsigned" SIZE_OF_UNSIGNED)
check_type_size("unsigned long" SIZE_OF_UNSIGNED_LONG)
check_type_size("unsigned long long" SIZE_OF_UNSIGNED_LONG_LONG)

check_type_size("__int64" __INT64)
check_type_size("unsigned __int64" UNSIGNED___INT64)

check_type_size(int16_t INT16_T)
check_type_size(int32_t INT32_T)
check_type_size(int64_t INT64_T)
check_type_size(intmax_t INTMAX_T)
check_type_size(uint8_t UINT8_T)
check_type_size(uint16_t UINT16_T)
check_type_size(uint32_t UINT32_T)
check_type_size(uint64_t UINT64_T)
check_type_size(uintmax_t UINTMAX_T)

#
set(CMAKE_EXTRA_INCLUDE_FILES time.h)
check_type_size(clock_t CLOCK_T)
if(NOT HAVE_CLOCK_T)
  set(clock_t int)
endif(NOT HAVE_CLOCK_T)
unset(CMAKE_EXTRA_INCLUDE_FILES)
#
set(CMAKE_EXTRA_INCLUDE_FILES time.h)
check_type_size(clockid_t CLOCKID_T)
if(NOT HAVE_CLOCKID_T)
  set(clockid_t int)
endif(NOT HAVE_CLOCKID_T)
unset(CMAKE_EXTRA_INCLUDE_FILES)
#
check_type_size(size_t SIZE_T)
if(NOT HAVE_SIZE_T)
  if("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
    set(size_t "uint64_t")
  else("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
    set(size_t   "uint32_t")
  endif("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
endif(NOT HAVE_SIZE_T)
#
check_type_size(ssize_t SSIZE_T)
if(NOT HAVE_SSIZE_T)
  if("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
    set(ssize_t "int64_t")
  else("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
    set(ssize_t "long")
  endif("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
endif(NOT HAVE_SSIZE_T)
#
check_type_size(pid_t PID_T)
if(NOT HAVE_PID_T)
  if(WIN32)
    set(pid_t "int")
  else(WIN32)
    MESSAGE(FATAL_ERROR "pid_t doesn't exist on this platform?")
  endif(WIN32)
endif(NOT HAVE_PID_T)
#
set(CMAKE_EXTRA_INCLUDE_FILES time.h)
check_type_size(timer_t TIMER_T)
if(NOT HAVE_TIMER_T)
  set(timer_t int)
endif(NOT HAVE_TIMER_T)
unset(CMAKE_EXTRA_INCLUDE_FILES)

###############################################################################
# Check libraries

check_library_exists(m floor "" HAVE_LIBM)
if (HAVE_LIBM)
  set (LIBM "m")
endif (HAVE_LIBM)

check_library_exists(rt clock_gettime "" HAVE_LIBRT)
if (HAVE_LIBRT)
  set(LIBRT "rt")
  ADD_DEFINITIONS(-DHAVE_LIBRT=1)
endif (HAVE_LIBRT)

check_library_exists(subunit subunit_test_start "" HAVE_SUBUNIT)
if (HAVE_SUBUNIT)
  set(SUBUNIT "subunit")
  set(ENABLE_SUBUNIT 1)
  add_definitions(-DENABLE_SUBUNIT=1)
else(HAVE_SUBUNIT)
  set(ENABLE_SUBUNIT 0)
  add_definitions(-DENABLE_SUBUNIT=0)
endif (HAVE_SUBUNIT)

###############################################################################
# Generate "config.h" from "cmake/config.h.in"
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/config.h)
  # Param @ONLY not used on purpose!
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
add_definitions(-DHAVE_CONFIG_H)
set(CONFIG_HEADER ${CMAKE_CURRENT_BINARY_DIR}/config.h)

###############################################################################
# Generate "check_stdint.h" from "cmake/check_stdint.h.in"
#
# The corresponding GNU Autotools build of this project
# has m4 macro `m4/ax_create_stdint_h.m4` to create
# the file `check_stdint.h` from scratch.
# Include file `stdint.h` was introduced in C99 ANSI standard but
# many compilers were lacking behind or still are and
# have not implemented C99 or their `stdint.h` is not compatible.
# Therefore the m4 macro was needed to create the required datatypes.
#
# When converting to CMake we also want to abandon the m4 macros.
#
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_stdint.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/check_stdint.h @ONLY)
if(NOT THIS_IS_SUBPROJECT)
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/check_stdint.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

###############################################################################
# Generate "check.pc", the package config (pkgconfig) file for libtool
if(NOT THIS_IS_SUBPROJECT)
  set(prefix_save "${PREFIX}")
  set(prefix "${CMAKE_INSTALL_PREFIX}")
  set(exec_prefix "\${prefix}")
  set(libdir "\${exec_prefix}/lib")
  set(includedir "\${prefix}/include")
  set(VERSION "${PROJECT_VERSION}")

  if (HAVE_SUBUNIT)
    set(LIBSUBUNIT_PC "libsubunit")
  else (HAVE_SUBINIT)
    set(LIBSUBUNIT_PC "")
  endif (HAVE_SUBUNIT)

  if (CHECK_ENABLE_GCOV)
    set(GCOV_LIBS "-lgcov")
  else (CHECK_ENABLE_GCOV)
    set(GCOV_LIBS "")
  endif (CHECK_ENABLE_GCOV)

  set(PTHREAD_LIBS "-pthread")
  set(LIBS "")

  if (HAVE_LIBM)
    set(LIBS "${LIBS} -lm")
  endif (HAVE_LIBM)

  if (HAVE_LIBRT)
    set(LIBS "${LIBS} -lrt")
  endif (HAVE_LIBRT)

  set(PTHREAD_CFLAGS "-pthread")

  configure_file(check.pc.in check.pc @ONLY)

  unset(PTHREAD_CFLAGS)
  unset(LIBS)
  unset(PTHREAD_LIBS)
  unset(GCOV_LIBS)
  unset(LIBSUBUNIT_PC)
  unset(VERSION)
  unset(includedir)
  unset(libdir)
  unset(exec_prefix)
  set(PREFIX "${prefix_save}")

  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/check.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
  )
endif()

###############################################################################
# Subdirectories
add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(checkmk)

###############################################################################
# Unit tests
if(BUILD_TESTING AND NOT THIS_IS_SUBPROJECT)
  add_subdirectory(tests)
  add_test(NAME check_check COMMAND check_check)
  add_test(NAME check_check_export COMMAND check_check_export)

  # Only offer to run shell scripts if we may have a working interpreter
  if(UNIX OR MINGW OR MSYS)
    add_test(NAME test_output.sh
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_output.sh)
    add_test(NAME test_log_output.sh
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_log_output.sh)
    add_test(NAME test_xml_output.sh
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xml_output.sh)
    add_test(NAME test_tap_output.sh
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_tap_output.sh)
    add_test(NAME test_check_nofork.sh
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_check_nofork.sh)
    add_test(NAME test_check_nofork_teardown.sh
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_check_nofork_teardown.sh)
    add_test(NAME test_set_max_msg_size.sh
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_set_max_msg_size.sh)
  endif(UNIX OR MINGW OR MSYS)
endif()

###############################################################################
# Export project, prepare a config and config-version files
if(NOT THIS_IS_SUBPROJECT)
  string(TOLOWER ${PROJECT_NAME} EXPORT_NAME)
  include(CMakePackageConfigHelpers)
  configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/${EXPORT_NAME}-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/${EXPORT_NAME}-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/${EXPORT_NAME}/cmake
  )
  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/${EXPORT_NAME}-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
  )

  export(EXPORT check-targets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/${EXPORT_NAME}-targets.cmake"
    NAMESPACE "${PROJECT_NAME}::"
  )

  install(EXPORT check-targets
    NAMESPACE "${PROJECT_NAME}::"
    FILE "${EXPORT_NAME}-targets.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${EXPORT_NAME}
  )
  install(
    FILES
      "${CMAKE_CURRENT_BINARY_DIR}/cmake/${EXPORT_NAME}-config.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/cmake/${EXPORT_NAME}-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${EXPORT_NAME}
  )
endif()

# Store the current build directory in the CMake user package registry.
# This helps dependent projects use a package from the current
# project’s build tree, i.e. without installing it.

# In CMake 3.14 and below the export(PACKAGE) command populated
# the user package registry by default and users needed to set
# the CMAKE_EXPORT_NO_PACKAGE_REGISTRY to disable it,
# e.g. in automated build and packaging environments. Since
# the user package registry is stored outside the build tree,
# this side effect should not be enabled by default.
# Therefore CMake 3.15 and above prefer that export(PACKAGE) does nothing
# unless an explicit CMAKE_EXPORT_PACKAGE_REGISTRY variable is set.
if(NOT THIS_IS_SUBPROJECT)
  export(PACKAGE "${PROJECT_NAME}")
endif()

# vim: shiftwidth=2:softtabstop=2:tabstop=2:expandtab:autoindent

