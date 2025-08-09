cmake_minimum_required(VERSION 3.15)

add_executable(sutf sutf.cpp)
add_executable(fastbase64 fastbase64.cpp)

if(SIMDUTF_ICONV)
find_package(Iconv)
if(Iconv_FOUND)
  message(STATUS "Iconv was found!")
  target_compile_definitions(sutf PRIVATE ICONV_AVAILABLE=1)
  if(Iconv_IS_BUILT_IN)
    message(STATUS "Iconv is part of the C library.")
  else(Iconv_IS_BUILT_IN)
    message(STATUS "Iconv is a separate library, headers at ${Iconv_INCLUDE_DIR}, binary at ${Iconv_LIBRARY}")
    target_link_libraries(sutf PUBLIC ${Iconv_LIBRARY})
    target_include_directories(sutf PUBLIC ${Iconv_INCLUDE_DIR})
  endif(Iconv_IS_BUILT_IN)
else(Iconv_FOUND)
  message(STATUS "Iconv was not found!")
endif(Iconv_FOUND)
else(SIMDUTF_ICONV)
  message(STATUS "We are not seeking iconv.")
endif(SIMDUTF_ICONV)

if(NOT (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"))
if((CMAKE_CXX_COMPILER_ID STREQUAL "GNU") OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
  execute_process(COMMAND ${CMAKE_CXX_COMPILER} -print-libgcc-file-name OUTPUT_VARIABLE SIMDUTF_GCC_LIB)
  get_filename_component(SIMDUTF_GCC_DIR "${SIMDUTF_GCC_LIB}" DIRECTORY)
  message(STATUS "looking for static C++ library in " ${SIMDUTF_GCC_DIR})
  find_library(LIBSTDCPP libstdc++.a PATHS "${SIMDUTF_GCC_DIR}")
  if(LIBSTDCPP)
    message(STATUS "libstdc++.a found")
    target_link_options(sutf PRIVATE "-static-libstdc++")
    target_link_options(fastbase64 PRIVATE "-static-libstdc++")
  else()
    message(STATUS "libstdc++.a not found")
  endif()
  target_link_options(sutf PRIVATE "-Wl,--gc-sections")
  target_link_options(fastbase64 PRIVATE "-Wl,--gc-sections")
endif()
endif()

message(STATUS "The tools require C++17. If your system does not support C++17, please set SIMDUTF_TOOLS to OFF.")
set_property(TARGET sutf PROPERTY CXX_STANDARD 17)
set_property(TARGET sutf PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET fastbase64 PROPERTY CXX_STANDARD 17)
set_property(TARGET fastbase64 PROPERTY CXX_STANDARD_REQUIRED ON)

target_link_libraries(sutf PUBLIC simdutf)
target_link_libraries(fastbase64 PUBLIC simdutf)

if(MSVC AND BUILD_SHARED_LIBS)
  # Copy the simdutf dll into the directory
  add_custom_command(TARGET sutf POST_BUILD        # Adds a post-build event
    COMMAND ${CMAKE_COMMAND} -E copy_if_different  # which executes "cmake -E copy_if_different..."
        "$<TARGET_FILE:simdutf>"      # <--this is in-file
        "$<TARGET_FILE_DIR:sutf>")                 # <--this is out-file path
endif()
install(
   TARGETS
    sutf
   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
   TARGETS
    fastbase64
   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)