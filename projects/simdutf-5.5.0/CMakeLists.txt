cmake_minimum_required(VERSION 3.15)

project(simdutf
  DESCRIPTION "Fast Unicode validation, transcoding and processing"
  LANGUAGES CXX
  VERSION 5.5.0
)

include (TestBigEndian)
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
if(IS_BIG_ENDIAN)
  message(STATUS "Big-endian system detected.")
endif()

include(GNUInstallDirs)
# The following requires CMake 3.21.
# if(PROJECT_IS_TOP_LEVEL)
#  message(STATUS "Building simdutf as a top-level project.")
#  include(CTest)
#else()
#  message(STATUS "Building simdutf as a subproject.")
#endif(PROJECT_IS_TOP_LEVEL)
include(CTest)
include(cmake/simdutf-flags.cmake)

set(SIMDUTF_LIB_VERSION "10.0.0" CACHE STRING "simdutf library version")
set(SIMDUTF_LIB_SOVERSION "10" CACHE STRING "simdutf library soversion")
option(SIMDUTF_TESTS "Whether the tests are included as part of the CMake Build." ON)
option(SIMDUTF_BENCHMARKS "Whether the benchmarks are included as part of the CMake Build." OFF)
option(SIMDUTF_TOOLS "Whether the tools are included as part of the CMake build. Requires C++17 or better." ON)
option(SIMDUTF_ICONV "Whether to use iconv as part of the CMake build if available." ON)
option(SIMDUTF_FUZZERS "Whether to build the fuzzers." Off)


set(SIMDUTF_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(src)
add_subdirectory(singleheader)

if (SIMDUTF_TESTS)
  message(STATUS "The tests are enabled.")
  add_subdirectory(tests)
else(SIMDUTF_TESTS)
  message(STATUS "The tests are disabled.")
endif(SIMDUTF_TESTS)


if (SIMDUTF_TOOLS)
  if(CMAKE_CXX_COMPILER_ID MATCHES GNU AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.0)
    message(STATUS "The benchmark tool requires GCC 8.0 or better.")
  else()
    add_subdirectory(tools)
  endif()
endif()


if (SIMDUTF_BENCHMARKS)
  if(NOT SIMDUTF_TESTS)
    message(STATUS "You have enabled the benchmarks, but not the tests. We don't allow running the benchmarks without the tests.")
  elif((CMAKE_CXX_COMPILER_ID MATCHES GNU) AND (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "8.0"))
    message(STATUS "The benchmark tool requires GCC 8.0 or better.")
  else()
    message(STATUS "The benchmarks can be disabled by setting SIMDUTF_BENCHMARKS, e.g., -D SIMDUTF_BENCHMARKS=OFF.")
    add_subdirectory(benchmarks)
  endif()
else()
  message(STATUS "The benchmarks can be enabled by setting SIMDUTF_BENCHMARKS, e.g., -D SIMDUTF_BENCHMARKS=ON.")
endif()

message(STATUS "Compiling using the C++ standard:" ${CMAKE_CXX_STANDARD})
# ---- Install rules ----
add_library(simdutf::simdutf ALIAS simdutf)

set_target_properties(
    simdutf PROPERTIES
    VERSION "${SIMDUTF_LIB_VERSION}"
    SOVERSION "${SIMDUTF_LIB_SOVERSION}"
    WINDOWS_EXPORT_ALL_SYMBOLS YES
)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

install(
    FILES include/simdutf.h
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT simdutf_Development
)

install(
    DIRECTORY include/simdutf
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT simdutf_Development
)

install(
    TARGETS simdutf
    EXPORT simdutfTargets
    RUNTIME COMPONENT simdutf_Runtime
    LIBRARY COMPONENT simdutf_Runtime
    NAMELINK_COMPONENT simdutf_Development
    ARCHIVE COMPONENT simdutf_Development
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

configure_file(cmake/simdutf-config.cmake.in simdutf-config.cmake @ONLY)

write_basic_package_version_file(
    simdutf-config-version.cmake
    COMPATIBILITY SameMinorVersion
)

set(
    SIMDUTF_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/simdutf"
    CACHE STRING "CMake package config location relative to the install prefix"
)
mark_as_advanced(SIMDUTF_INSTALL_CMAKEDIR)

install(
    FILES
    "${PROJECT_BINARY_DIR}/simdutf-config.cmake"
    "${PROJECT_BINARY_DIR}/simdutf-config-version.cmake"
    DESTINATION "${SIMDUTF_INSTALL_CMAKEDIR}"
    COMPONENT simdutf_Development
)

install(
    EXPORT simdutfTargets
    NAMESPACE simdutf::
    DESTINATION "${SIMDUTF_INSTALL_CMAKEDIR}"
    COMPONENT example_Development
)


# pkg-config
include(cmake/JoinPaths.cmake)
join_paths(PKGCONFIG_INCLUDEDIR "\${prefix}" "${CMAKE_INSTALL_INCLUDEDIR}")
join_paths(PKGCONFIG_LIBDIR "\${prefix}" "${CMAKE_INSTALL_LIBDIR}")

configure_file("simdutf.pc.in" "simdutf.pc" @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/simdutf.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)
#
# CPack
#
if(is_top_project)
  set(CPACK_PACKAGE_VENDOR "Daniel Lemire")
  set(CPACK_PACKAGE_CONTACT "lemire@gmail.com")
  set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE-MIT")
  set(CPACK_RPM_PACKAGE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE-MIT")
  set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README.md")
  set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
  include(CPack)
endif()

# ----



if(NOT SIMDUTF_SANITIZE)
  find_program(GREP grep)
  find_program(NM nm)
  if((NOT GREP) OR (NOT NM))
    message("grep and nm are unavailable on this system.")
  else()
    add_test(
      NAME "avoid_abort"
      # Under FreeBSD, the __cxa_guard_abort symbol may appear but it is fine.
      # So we want to look for <space><possibly _>abort as a test.
      COMMAND sh -c "${NM}  $<TARGET_FILE_NAME:simdutf> |  ${GREP} ' _*abort' || exit 0  && exit 1"
      WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    )
    add_test(
      NAME "avoid_cout"
      COMMAND sh -c "${NM}  $<TARGET_FILE_NAME:simdutf> |  ${GREP} ' _*cout' || exit 0  && exit 1"
      WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    )
    add_test(
      NAME "avoid_cerr"
      COMMAND sh -c "${NM}  $<TARGET_FILE_NAME:simdutf> |  ${GREP} ' _*cerr' || exit 0  && exit 1"
      WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    )
    add_test(
      NAME "avoid_printf"
      COMMAND sh -c "${NM}  $<TARGET_FILE_NAME:simdutf> |  ${GREP} ' _*printf' || exit 0  && exit 1"
      WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    )
    add_test(
      NAME "avoid_stdout"
      COMMAND sh -c "${NM}  $<TARGET_FILE_NAME:simdutf> |  ${GREP} stdout || exit 0 && exit 1"
      WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    )
    add_test(
      NAME "avoid_stderr"
      COMMAND sh -c "${NM}  $<TARGET_FILE_NAME:simdutf> |  ${GREP} stderr || exit 0 && exit 1"
      WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    )
  endif()
endif()

if(SIMDUTF_FUZZERS)
    add_subdirectory(fuzz)
endif()
