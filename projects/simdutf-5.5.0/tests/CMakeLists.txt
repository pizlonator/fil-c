set(SIMDUTF_TEST_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../)
if(MSVC)
  add_compile_options("/Zi" "/EHsc" "/GR")
endif()

add_subdirectory(helpers)
add_subdirectory(reference)

include(${PROJECT_SOURCE_DIR}/cmake/add_cpp_test.cmake)

add_cpp_test(null_safety_tests)
target_link_libraries(null_safety_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(readme_tests)
target_link_libraries(readme_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(random_fuzzer)
target_link_libraries(random_fuzzer
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)
if(MSVC AND BUILD_SHARED_LIBS)
  # Copy the simdutf dll into the tests directory
  add_custom_command(TARGET random_fuzzer POST_BUILD        # Adds a post-build event
    COMMAND ${CMAKE_COMMAND} -E copy_if_different  # which executes "cmake -E copy_if_different..."
        "$<TARGET_FILE:simdutf>"      # <--this is in-file
        "$<TARGET_FILE_DIR:random_fuzzer>")                 # <--this is out-file path
endif()
add_cpp_test(special_tests)
target_link_libraries(special_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(validate_ascii_basic_tests)
target_link_libraries(validate_ascii_basic_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(validate_ascii_with_errors_tests)
target_link_libraries(validate_ascii_with_errors_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(bele_tests)
target_link_libraries(bele_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(validate_utf8_basic_tests)
target_link_libraries(validate_utf8_basic_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)


add_cpp_test(select_implementation)
target_link_libraries(select_implementation
  PUBLIC simdutf::tests::helpers
        simdutf::tests::reference)

add_cpp_test(validate_utf8_brute_force_tests)
target_link_libraries(validate_utf8_brute_force_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(validate_utf8_puzzler_tests)
target_link_libraries(validate_utf8_puzzler_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(validate_utf8_with_errors_tests)
target_link_libraries(validate_utf8_with_errors_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(validate_utf16le_basic_tests)
target_link_libraries(validate_utf16le_basic_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(validate_utf16be_basic_tests)
target_link_libraries(validate_utf16be_basic_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(validate_utf16le_with_errors_tests)
target_link_libraries(validate_utf16le_with_errors_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(validate_utf16be_with_errors_tests)
target_link_libraries(validate_utf16be_with_errors_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(validate_utf32_basic_tests)
target_link_libraries(validate_utf32_basic_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(validate_utf32_with_errors_tests)
target_link_libraries(validate_utf32_with_errors_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  set(SCRIPT ${SIMDUTF_SOURCE_DIR}/scripts/sse_validate_utf16le_testcases.py)
  add_custom_command(TARGET validate_utf16le_basic_tests
                   COMMAND ${Python3_EXECUTABLE} ${SCRIPT}
                   DEPENDS ${SCRIPT}
                   VERBATIM
  )
  unset(SCRIPT)
endif(Python3_FOUND)


add_cpp_test(convert_latin1_to_utf8_tests)
target_link_libraries(convert_latin1_to_utf8_tests 
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_latin1_to_utf16le_tests)
target_link_libraries(convert_latin1_to_utf16le_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_latin1_to_utf16be_tests)
target_link_libraries(convert_latin1_to_utf16be_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_latin1_to_utf32_tests)
target_link_libraries(convert_latin1_to_utf32_tests 
    PUBLIC simdutf::tests::helpers
      simdutf::tests::reference)

add_cpp_test(convert_utf8_to_latin1_tests)
target_link_libraries(convert_utf8_to_latin1_tests 
  PUBLIC simdutf::tests::helpers
        simdutf::tests::reference)

add_cpp_test(convert_utf8_to_latin1_with_errors_tests)
target_link_libraries(convert_utf8_to_latin1_with_errors_tests 
  PUBLIC simdutf::tests::helpers
          simdutf::tests::reference)
        
add_cpp_test(convert_valid_utf8_to_latin1_tests)
target_link_libraries(convert_valid_utf8_to_latin1_tests 
  PUBLIC simdutf::tests::helpers
        simdutf::tests::reference)
         
add_cpp_test(convert_valid_utf8_to_utf16le_tests)
target_link_libraries(convert_valid_utf8_to_utf16le_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_valid_utf8_to_utf16be_tests)
target_link_libraries(convert_valid_utf8_to_utf16be_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_valid_utf8_to_utf32_tests)
target_link_libraries(convert_valid_utf8_to_utf32_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_utf8_to_utf16le_tests)
target_link_libraries(convert_utf8_to_utf16le_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_utf8_to_utf16be_tests)
target_link_libraries(convert_utf8_to_utf16be_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_utf8_to_utf16le_with_errors_tests)
target_link_libraries(convert_utf8_to_utf16le_with_errors_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_utf8_to_utf16be_with_errors_tests)
target_link_libraries(convert_utf8_to_utf16be_with_errors_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_utf8_to_utf32_tests)
target_link_libraries(convert_utf8_to_utf32_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_utf8_to_utf32_with_errors_tests)
target_link_libraries(convert_utf8_to_utf32_with_errors_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_utf16le_to_latin1_tests)
target_link_libraries(convert_utf16le_to_latin1_tests 
  PUBLIC simdutf::tests::helpers
          simdutf::tests::reference)

add_cpp_test(convert_utf16be_to_latin1_tests)
target_link_libraries(convert_utf16be_to_latin1_tests 
  PUBLIC simdutf::tests::helpers
          simdutf::tests::reference)

add_cpp_test(convert_utf16le_to_latin1_tests_with_errors)
target_link_libraries(convert_utf16le_to_latin1_tests_with_errors 
  PUBLIC simdutf::tests::helpers
          simdutf::tests::reference)

add_cpp_test(convert_utf16be_to_latin1_tests_with_errors)
target_link_libraries(convert_utf16be_to_latin1_tests_with_errors 
  PUBLIC simdutf::tests::helpers
          simdutf::tests::reference)

add_cpp_test(convert_valid_utf16le_to_latin1_tests)
target_link_libraries(convert_valid_utf16le_to_latin1_tests 
  PUBLIC simdutf::tests::helpers
          simdutf::tests::reference)

add_cpp_test(convert_valid_utf16be_to_latin1_tests)
target_link_libraries(convert_valid_utf16be_to_latin1_tests 
  PUBLIC simdutf::tests::helpers
          simdutf::tests::reference)
                
add_cpp_test(convert_utf16le_to_utf8_tests)
target_link_libraries(convert_utf16le_to_utf8_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_utf16be_to_utf8_tests)
target_link_libraries(convert_utf16be_to_utf8_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_utf16le_to_utf8_with_errors_tests)
target_link_libraries(convert_utf16le_to_utf8_with_errors_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_utf16be_to_utf8_with_errors_tests)
target_link_libraries(convert_utf16be_to_utf8_with_errors_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)
        
add_cpp_test(convert_utf32_to_latin1_tests)
target_link_libraries(convert_utf32_to_latin1_tests 
  PUBLIC simdutf::tests::helpers
        simdutf::tests::reference)

add_cpp_test(convert_valid_utf32_to_latin1_tests)
target_link_libraries(convert_valid_utf32_to_latin1_tests 
  PUBLIC simdutf::tests::helpers
        simdutf::tests::reference)

add_cpp_test(convert_utf32_to_latin1_with_errors_tests)
target_link_libraries(convert_utf32_to_latin1_with_errors_tests 
  PUBLIC simdutf::tests::helpers
        simdutf::tests::reference)         
          
add_cpp_test(convert_utf32_to_utf8_tests)
target_link_libraries(convert_utf32_to_utf8_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_utf32_to_utf8_with_errors_tests)
target_link_libraries(convert_utf32_to_utf8_with_errors_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_utf32_to_utf16le_tests)
target_link_libraries(convert_utf32_to_utf16le_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_utf32_to_utf16be_tests)
target_link_libraries(convert_utf32_to_utf16be_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_utf32_to_utf16le_with_errors_tests)
target_link_libraries(convert_utf32_to_utf16le_with_errors_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_utf32_to_utf16be_with_errors_tests)
target_link_libraries(convert_utf32_to_utf16be_with_errors_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_valid_utf16le_to_utf8_tests)
target_link_libraries(convert_valid_utf16le_to_utf8_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_valid_utf16be_to_utf8_tests)
target_link_libraries(convert_valid_utf16be_to_utf8_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_valid_utf32_to_utf8_tests)
target_link_libraries(convert_valid_utf32_to_utf8_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_valid_utf32_to_utf16le_tests)
target_link_libraries(convert_valid_utf32_to_utf16le_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_valid_utf32_to_utf16be_tests)
target_link_libraries(convert_valid_utf32_to_utf16be_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_utf16le_to_utf32_tests)
target_link_libraries(convert_utf16le_to_utf32_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_utf16be_to_utf32_tests)
target_link_libraries(convert_utf16be_to_utf32_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_utf16le_to_utf32_with_errors_tests)
target_link_libraries(convert_utf16le_to_utf32_with_errors_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_utf16be_to_utf32_with_errors_tests)
target_link_libraries(convert_utf16be_to_utf32_with_errors_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_valid_utf16le_to_utf32_tests)
target_link_libraries(convert_valid_utf16le_to_utf32_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(convert_valid_utf16be_to_utf32_tests)
target_link_libraries(convert_valid_utf16be_to_utf32_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(count_utf8)
target_link_libraries(count_utf8
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(count_utf16le)
target_link_libraries(count_utf16le
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(count_utf16be)
target_link_libraries(count_utf16be
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(detect_encodings_tests)
target_link_libraries(detect_encodings_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(basic_fuzzer)
target_link_libraries(basic_fuzzer
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)

add_cpp_test(base64_tests)
target_link_libraries(base64_tests
  PUBLIC simdutf::tests::helpers
         simdutf::tests::reference)
