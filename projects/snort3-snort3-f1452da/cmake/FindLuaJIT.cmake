#
# Locate Lua library (hacked to accept regular Lua in place of LuaJIT)
# This module defines
#  LUAJIT_FOUND, if false, do not try to link to Lua
#  LUAJIT_LIBRARIES
#  LUAJIT_INCLUDE_DIR, where to find lua.h
#  LUAJIT_VERSION_STRING, the version of Lua found

set(ERROR_MESSAGE
    "\n\tCan't Find Lua!  Use the --with-luajit-*
    options if you have it installed in an unusual place.\n"
)

# Use LUAJIT_INCLUDE_DIR_HINT and LUAJIT_LIBRARY_DIR_HINT from configure_cmake.sh as primary hints.
# Look for lua.h instead of luajit.h.
find_path(LUAJIT_INCLUDE_DIR lua.h
    HINTS ${LUAJIT_INCLUDE_DIR_HINT} ${CMAKE_PREFIX_PATH}/include/lua5.4 ${CMAKE_PREFIX_PATH}/include)
if (STATIC_LUAJIT)
  find_library(LUAJIT_LIBRARIES NAMES liblua5.4.a liblua.a
    HINTS ${LUAJIT_LIBRARIES_DIR_HINT} ${CMAKE_PREFIX_PATH}/lib)
else()
  find_library(LUAJIT_LIBRARIES NAMES lua5.4 lua
    HINTS ${LUAJIT_LIBRARIES_DIR_HINT} ${CMAKE_PREFIX_PATH}/lib)
endif()

# Extract version from lua.h instead of luajit.h
if(LUAJIT_INCLUDE_DIR AND EXISTS "${LUAJIT_INCLUDE_DIR}/lua.h")
    file(STRINGS "${LUAJIT_INCLUDE_DIR}/lua.h" lua_version_major REGEX "^#define[ \t]+LUA_VERSION_MAJOR[ \t]+\".+\"")
    file(STRINGS "${LUAJIT_INCLUDE_DIR}/lua.h" lua_version_minor REGEX "^#define[ \t]+LUA_VERSION_MINOR[ \t]+\".+\"")
    file(STRINGS "${LUAJIT_INCLUDE_DIR}/lua.h" lua_version_release REGEX "^#define[ \t]+LUA_VERSION_RELEASE[ \t]+\".+\"")

    string(REGEX REPLACE "^#define[ \t]+LUA_VERSION_MAJOR[ \t]+\"([^\"]+)\".*" "\\1" LUA_VMAJ "${lua_version_major}")
    string(REGEX REPLACE "^#define[ \t]+LUA_VERSION_MINOR[ \t]+\"([^\"]+)\".*" "\\1" LUA_VMIN "${lua_version_minor}")
    string(REGEX REPLACE "^#define[ \t]+LUA_VERSION_RELEASE[ \t]+\"([^\"]+)\".*" "\\1" LUA_VREL "${lua_version_release}")

    set(LUAJIT_VERSION_STRING "${LUA_VMAJ}.${LUA_VMIN}.${LUA_VREL}")
    unset(lua_version_major)
    unset(lua_version_minor)
    unset(lua_version_release)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LuaJIT
    REQUIRED_VARS LUAJIT_LIBRARIES LUAJIT_INCLUDE_DIR
    VERSION_VAR LUAJIT_VERSION_STRING
    FAIL_MESSAGE "${ERROR_MESSAGE}"
)

mark_as_advanced(LUAJIT_INCLUDE_DIR LUAJIT_LIBRARIES)
