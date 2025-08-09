add_library(simdutf_tests_reference STATIC
  encode_utf8.h
  encode_utf8.cpp
  encode_utf16.h
  encode_utf16.cpp
  encode_utf32.h
  encode_utf32.cpp
  encode_latin1.h
  encode_latin1.cpp
  validate_utf8_to_latin1.h
  validate_utf8_to_latin1.cpp
  validate_utf16_to_latin1.h
  validate_utf16_to_latin1.cpp
  validate_utf32_to_latin1.h
  validate_utf32_to_latin1.cpp
  validate_utf8.h
  validate_utf8.cpp
  validate_utf16.h
  validate_utf16.cpp
  validate_utf32.h
  validate_utf32.cpp)
add_library(simdutf::tests::reference ALIAS simdutf_tests_reference)
target_link_libraries(simdutf_tests_reference PRIVATE simdutf)
