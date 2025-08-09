#
# Amalgamation
#
set(SINGLEHEADER_FILES
  ${CMAKE_CURRENT_BINARY_DIR}/simdutf.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/simdutf.h
  ${CMAKE_CURRENT_BINARY_DIR}/amalgamation_demo.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/README.md
)
set_source_files_properties(${SINGLEHEADER_FILES} PROPERTIES GENERATED TRUE)

# In theory, this is unneeded, because the tests module does the same test:
find_package (Python3 COMPONENTS Interpreter)

if (Python3_Interpreter_FOUND)
  MESSAGE( STATUS "Python found, we are going to amalgamate.py." )

  add_custom_command(
    OUTPUT ${SINGLEHEADER_FILES}
    COMMAND ${CMAKE_COMMAND} -E env
      AMALGAMATE_SOURCE_PATH=${PROJECT_SOURCE_DIR}/src
      AMALGAMATE_INPUT_PATH=${PROJECT_SOURCE_DIR}/include
      AMALGAMATE_OUTPUT_PATH=${CMAKE_CURRENT_BINARY_DIR}
      ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/amalgamate.py
      #
      # This is the best way I could find to make amalgamation trigger whenever source files or
      # header files change: since the "simdutf" library has to get rebuilt when that happens, we
      # take a dependency on the generated library file (even though we're not using it). Depending
      # on simdutf-source doesn't do the trick because DEPENDS here can only depend on an
      # *artifact*--it won't scan source and include files the way a concrete library or executable
      # will.
      #
      # It sucks that we have to build the actual library to make it happen, but it's better than\
      # nothing!
      #
      DEPENDS amalgamate.py simdutf
  )
  add_custom_target(simdutf-singleheader-files DEPENDS ${SINGLEHEADER_FILES})

  #
  # Include this if you intend to #include "simdutf.cpp" in your own .cpp files.
  #
  add_library(simdutf-singleheader-include-source INTERFACE)
  target_include_directories(simdutf-singleheader-include-source INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
  add_dependencies(simdutf-singleheader-include-source simdutf-singleheader-files)

  add_library(simdutf-singleheader-source INTERFACE)
  target_sources(simdutf-singleheader-source INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/simdutf.cpp>)
  target_link_libraries(simdutf-singleheader-source INTERFACE simdutf-singleheader-include-source)
  if (SIMDUTF_TESTS)
    add_executable(amalgamation_demo $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/amalgamation_demo.cpp>)
    target_link_libraries(amalgamation_demo simdutf-singleheader-include-source)
    if (CMAKE_CROSSCOMPILING_EMULATOR)
      add_test(amalgamation_demo ${CMAKE_CROSSCOMPILING_EMULATOR} amalgamation_demo)
    else()
      add_test(amalgamation_demo amalgamation_demo)
    endif()
    if(NOT MSVC)
      set_source_files_properties($<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/simdutf.cpp> PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Weffc++;-Wfatal-errors;-Wsign-compare;-Wshadow;-Wwrite-strings;-Wpointer-arith;-Winit-self;-Wconversion;-Wno-sign-conversion;-Wunused-function")
    endif()
  endif()
else()
  MESSAGE( STATUS "Python not found, we are unable to test amalgamate.py." )
endif()
