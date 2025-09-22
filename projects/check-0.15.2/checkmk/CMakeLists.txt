#
# Check: a unit test framework for C
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

set(conf_file "checkmk.in" FILEPATH)
set(configure_input "Generated from ${conf_file} by configure.")
find_program(AWK_PATH awk)

configure_file(checkmk.in checkmk @ONLY)

file(COPY doc/checkmk.1 DESTINATION man/man1)

option(INSTALL_CHECKMK "Install checkmk" ON)
if(INSTALL_CHECKMK AND NOT THIS_IS_SUBPROJECT)
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/checkmk
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  )
  install(
    DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/man
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}
  )
endif()

# vim: shiftwidth=2:softtabstop=2:tabstop=2:expandtab:autoindent

