add_library(simdutf_tests_helpers STATIC
  test.h
  test.cpp
  random_int.h
  random_int.cpp
  transcode_test_base.h
  transcode_test_base.cpp
  random_utf8.h
  random_utf8.cpp
  random_utf16.h
  random_utf16.cpp
  random_utf32.h
  random_utf32.cpp)
target_include_directories(simdutf_tests_helpers PUBLIC ${SIMDUTF_TEST_INCLUDE_DIR})
target_link_libraries(simdutf_tests_helpers PUBLIC simdutf simdutf::tests::reference)

add_library(simdutf::tests::helpers ALIAS simdutf_tests_helpers)
