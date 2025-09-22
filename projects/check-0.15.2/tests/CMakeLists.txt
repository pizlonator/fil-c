#
# Check: a unit test framework for C
#
# Copyright (C) 2011 Mateusz Loskot
# Copyright (C) 2001, 2002 Arien Malec
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

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src)
# Enable finding check.h
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../src)

# For the test_vars.in script, to give the unit test shell script
# runners the location of the source files
set(srcdir "${CMAKE_CURRENT_SOURCE_DIR}")

if(WIN32)
  # CMake uses Unix slashes for everything, but the tests that
  # read srcdir expect platform specific slashes. There are two
  # slashes because the shell scripts will consume srcdir.
  string(REPLACE "/" "\\\\" srcdir "${srcdir}")
  set(EXEEXT ".exe")
  set(IS_MSVC "1")
endif(WIN32)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/test_vars.in
  ${CMAKE_CURRENT_BINARY_DIR}/test_vars @ONLY)

if(ENABLE_MEMORY_LEAKING_TESTS)
  add_definitions(-DMEMORY_LEAKING_TESTS_ENABLED=1)
else(ENABLE_MEMORY_LEAKING_TESTS)
  add_definitions(-DMEMORY_LEAKING_TESTS_ENABLED=0)
endif(ENABLE_MEMORY_LEAKING_TESTS)

set(CHECK_CHECK_SOURCES
  check_check_exit.c
  check_check_fixture.c
  check_check_fork.c
  check_check_limit.c
  check_check_log.c
  check_check_log_internal.c
  check_check_main.c
  check_check_master.c
  check_check_msg.c
  check_check_pack.c
  check_check_selective.c
  check_check_sub.c
  check_check_tags.c
  check_list.c)
set(CHECK_CHECK_HEADERS check_check.h)
add_executable(check_check ${CHECK_CHECK_HEADERS} ${CHECK_CHECK_SOURCES})
target_link_libraries(check_check check)

set(CHECK_CHECK_EXPORT_SOURCES
  check_check_sub.c
  check_check_master.c
  check_check_log.c
  check_check_fork.c
  check_check_export_main.c
)
set(CHECK_CHECK_EXPORT_HEADERS check_check.h)
add_executable(check_check_export
  ${CHECK_CHECK_EXPORT_HEADERS}
  ${CHECK_CHECK_EXPORT_SOURCES})
target_link_libraries(check_check_export check)

set(EX_OUTPUT_SOURCES ex_output.c)
add_executable(ex_output ${EX_OUTPUT_SOURCES})
target_link_libraries(ex_output check)

set(CHECK_NOFORK_SOURCES check_nofork.c)
add_executable(check_nofork ${CHECK_NOFORK_SOURCES})
target_link_libraries(check_nofork check)

set(CHECK_NOFORK_TEARDOWN_SOURCES check_nofork_teardown.c)
add_executable(check_nofork_teardown ${CHECK_NOFORK_TEARDOWN_SOURCES})
target_link_libraries(check_nofork_teardown check)

set(CHECK_SET_MAX_MSG_SIZE_SOURCES check_set_max_msg_size.c)
add_executable(check_set_max_msg_size ${CHECK_SET_MAX_MSG_SIZE_SOURCES})
target_link_libraries(check_set_max_msg_size check)
