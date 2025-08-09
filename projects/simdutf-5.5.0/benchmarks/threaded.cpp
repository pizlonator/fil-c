/**
 * This simple program is meant to test our ability to multithread
 */

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

#include "simdutf.h"

uint64_t nano() {
  return std::chrono::duration_cast<::std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

template <typename PROCEDURE>
double bench(PROCEDURE f, uint64_t threshold = 200'000'000) {
  uint64_t start = nano();
  uint64_t finish = start;
  size_t times{0};
  for (; finish - start < threshold; times++) {
    size_t output_size = f();
    if (output_size == 0) {
      throw std::runtime_error("bad input?");
    }
    finish = nano();
  }
  return double(finish - start) / times;
}

void run_from_utf8(const std::vector<char> &input_data) {
  std::cout << "# input size = " << input_data.size() << " bytes" << std::endl;

  std::vector<char16_t> buffer(
      simdutf::utf16_length_from_utf8(input_data.data(), input_data.size()));
  // Let us warm up:
  bench(
      [&input_data, &buffer]() -> size_t {
        return simdutf::convert_utf8_to_utf16le(
            input_data.data(), input_data.size(), buffer.data());
      },
      2'000'000'000);
  // warmed up
  auto singled_procedure = [&input_data, &buffer]() -> size_t {
    return simdutf::convert_utf8_to_utf16le(input_data.data(),
                                            input_data.size(), buffer.data());
  };
  size_t counter = 0;
  auto just_thread_procedure = [&counter]() -> size_t {
    auto mythread = std::thread([&counter] { counter++; });
    mythread.join();
    return counter;
  };
  std::cout << "just thread: \t";
  double time_ns = bench(just_thread_procedure);
  std::cout << time_ns << "\n";

  std::cout << "singlethread: \t";

  time_ns = bench(singled_procedure);

  std::cout << time_ns << "\n";

  // split the input
  size_t midpoint = input_data.size() / 2;
  while ((input_data[midpoint] & 0b11100000) == 0b10000000) {
    midpoint--;
  }
  size_t offset = simdutf::utf16_length_from_utf8(input_data.data(), midpoint);
  auto double_procedure = [&input_data, &midpoint, &offset,
                           &buffer]() -> size_t {
    size_t output2 = 0;
    auto mythread =
        std::thread([&output2, &input_data, &buffer, midpoint, offset] {
          output2 = simdutf::convert_utf8_to_utf16le(
              input_data.data() + midpoint, input_data.size() - midpoint,
              buffer.data() + offset);
        });
    size_t output1 = simdutf::convert_utf8_to_utf16le(input_data.data(),
                                                      midpoint, buffer.data());
    mythread.join();
    return output2 + output1;
  };
  std::cout << "doublethread: \t";

  time_ns = bench(double_procedure);

  std::cout << time_ns << "\n";
}

int main(int argc, char **argv) {
  printf("# current system detected as %s.\n",
         simdutf::get_active_implementation()->name().c_str());
  if (argc < 2) {
    std::cerr << "Please provide a file argument." << std::endl;
    return EXIT_FAILURE;
  }
  const char *filename = argv[1];
  printf("# loading file %s\n", filename);
  std::filesystem::path path(filename);
  std::ifstream file;
  file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  file.open(path);
  std::vector<char> input_data;
  input_data.assign(std::istreambuf_iterator<char>(file),
                    std::istreambuf_iterator<char>());
  auto detected_encoding =
      simdutf::autodetect_encoding(input_data.data(), input_data.size());
  printf("# input detected as %s.\n",
         simdutf::to_string(detected_encoding).c_str());
  if (detected_encoding == simdutf::encoding_type::UTF8) {

    run_from_utf8(input_data);
  } else {
    printf("We only support UTF-8 inputs.\n");
  }
  return EXIT_SUCCESS;
}
