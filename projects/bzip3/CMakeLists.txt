cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

project(
  bzip3
  VERSION 1.5.1
  DESCRIPTION "A better and stronger spiritual successor to BZip2"
  HOMEPAGE_URL "https://github.com/kspalaiologos/bzip3"
  LANGUAGES C)

set(CMAKE_C_STANDARD 99)

option(BUILD_SHARED_LIBS "Build libbz3 as a shared library" ON)
option(BZIP3_BUILD_APPS "Build bzip3 applications" ON)
option(BZIP3_ENABLE_PTHREAD "Enable use of pthread library" ON)
option(BZIP3_ENABLE_ARCH_NATIVE "Enable CPU-specific optimizations" OFF)
option(BZIP3_ENABLE_STATIC_EXE "Enable static builds of the executable" OFF)

include(CheckCCompilerFlag)
include(CheckSymbolExists)
include(GNUInstallDirs)

set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${CMAKE_INSTALL_PREFIX})
set(bindir ${CMAKE_INSTALL_FULL_BINDIR})
set(libdir ${CMAKE_INSTALL_FULL_LIBDIR})
set(includedir ${CMAKE_INSTALL_FULL_INCLUDEDIR})
set(PACKAGE ${CMAKE_PROJECT_NAME})
set(PACKAGE_VERSION ${PROJECT_VERSION})
configure_file(bzip3.pc.in ${CMAKE_CURRENT_BINARY_DIR}/bzip3.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bzip3.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

if(BZIP3_ENABLE_PTHREAD)
  set(THREADS_PREFER_PTHREAD_FLAG TRUE)
  find_package(Threads REQUIRED)
endif()

if(BUILD_SHARED_LIBS)
  add_library(bz3 SHARED)
else()
  add_library(bz3 STATIC)
endif()
target_sources(bz3 PRIVATE src/libbz3.c)
target_compile_definitions(bz3 PUBLIC VERSION="${PROJECT_VERSION}")
target_include_directories(
  bz3
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
if(BZIP3_ENABLE_PTHREAD)
  target_compile_definitions(bz3 PUBLIC PTHREAD)
  target_link_libraries(bz3 Threads::Threads)
endif()
if(BZIP3_ENABLE_ARCH_NATIVE)
  check_c_compiler_flag(-march=native CC_SUPPORT_MARCH_NATIVE_FLAG)
  check_c_compiler_flag(-mtune=native CC_SUPPORT_MTUNE_NATIVE_FLAG)
  if(CC_SUPPORT_MARCH_NATIVE_FLAG AND CC_SUPPORT_MTUNE_NATIVE_FLAG)
    target_link_options(bz3 PUBLIC -march=native -mtune=native)
  else()
    message(
      FATAL_ERROR
        "Compiler does not support native optimizations, disable `BZIP3_ENABLE_ARCH_NATIVE`"
    )
  endif()
endif()
set_target_properties(
  bz3
  PROPERTIES OUTPUT_NAME bzip3
             SOVERSION "0.0.0"
             PUBLIC_HEADER include/libbz3.h
             VERSION "0")
if(BUILD_SHARED_LIBS)
  set_target_properties(bz3 PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
install(
  TARGETS bz3
  EXPORT ${CMAKE_PROJECT_NAME}-config
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
  EXPORT ${CMAKE_PROJECT_NAME}-config
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${CMAKE_PROJECT_NAME}
  NAMESPACE ${CMAKE_PROJECT_NAME}::)

if(BZIP3_BUILD_APPS)
  add_executable(bzip3)
  target_sources(bzip3 PRIVATE src/main.c)
  if(BZIP3_ENABLE_STATIC_EXE)
    if(BUILD_SHARED_LIBS)
      message(
        FATAL_ERROR
          "libbz3 is not built as a static library, disable `BUILD_SHARED_LIBS`"
      )
    endif()
    check_c_compiler_flag(-static CC_SUPPORT_STATIC_FLAG)
    if(CC_SUPPORT_STATIC_FLAG)
      target_link_options(bzip3 PRIVATE -static)
    else()
      message(
        FATAL_ERROR
          "Compiler does not support static linking, disable `BZIP3_ENABLE_STATIC_EXE`"
      )
    endif()
  endif()
  target_link_libraries(bzip3 PRIVATE bz3)
  install(TARGETS bzip3 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

  set(BZIP3_APP_SCRIPTS bunzip3 bz3cat bz3grep bz3less bz3more bz3most)
  install(PROGRAMS ${BZIP3_APP_SCRIPTS} DESTINATION ${CMAKE_INSTALL_BINDIR})

  if(UNIX)
    set(BZIP3_MANS
        bunzip3.1
        bz3cat.1
        bz3grep.1
        bz3less.1
        bz3more.1
        bz3most.1
        bzip3.1)
    foreach(BZIP3_MAN ${BZIP3_MANS})
      if(EXISTS ${BZIP3_MAN}.in)
        string(TIMESTAMP MAN_DATE "%d %B %Y" UTC)
        set(TRANSFORMED_PACKAGE_NAME ${CMAKE_PROJECT_NAME})
        set(MAN_DATE ${MAN_DATE})
        set(VERSION ${PROJECT_VERSION})
        configure_file(${BZIP3_MAN}.in ${CMAKE_CURRENT_BINARY_DIR}/${BZIP3_MAN}
                       @ONLY)
      else()
        configure_file(${BZIP3_MAN} ${CMAKE_CURRENT_BINARY_DIR}/${BZIP3_MAN}
                       COPYONLY)
      endif()
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${BZIP3_MAN}
              DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
    endforeach()
  endif()
endif()
