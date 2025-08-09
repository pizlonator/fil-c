/**
 * This simple program is meant to test the effect of alignment on the speed.
 */

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
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
  std::cout << "# aligning input" << std::endl;
  std::cout << "# input size = " << input_data.size() << " bytes" << std::endl;
  std::vector<char> tmp_buffer(input_data);
  tmp_buffer.resize(tmp_buffer.size() + 512);
  std::vector<char16_t> buffer(
      simdutf::utf16_length_from_utf8(input_data.data(), input_data.size()));
  size_t utf8count = simdutf::count_utf8(input_data.data(), input_data.size());
  // Let us warm up:
  bench(
      [&tmp_buffer, &input_data, &buffer]() -> size_t {
        return simdutf::convert_utf8_to_utf16le(
            tmp_buffer.data(), input_data.size(), buffer.data());
      },
      2'000'000'000);
  // warmed up
  double best_speed = 0;
  double worst_speed = 1e300;

  for (size_t offset = 0; offset < 512; offset += 1) {
    memcpy(tmp_buffer.data() + offset, input_data.data(), input_data.size());

    std::cout << offset << ",\t";
    auto size_procedure = [&tmp_buffer, &input_data, offset,
                           &buffer]() -> size_t {
      return simdutf::convert_utf8_to_utf16le(tmp_buffer.data() + offset,
                                              input_data.size(), buffer.data());
    };
    double speed = utf8count / bench(size_procedure);
    if (speed > best_speed) {
      best_speed = speed;
    }
    if (speed < worst_speed) {
      worst_speed = speed;
    }

    std::cout << speed << "\n";
  }
  std::cout << "# worst speed " << worst_speed << std::endl;
  std::cout << "# best speed " << best_speed << std::endl;
  std::cout << "# difference (percent) " << (best_speed / worst_speed - 1) * 100
            << std::endl;
}


void run_from_utf8_output(const std::vector<char> &input_data) {
  std::cout << "# aligning output" << std::endl;
  std::cout << "# input size = " << input_data.size() << " bytes" << std::endl;

  std::vector<char16_t> buffer(
      simdutf::utf16_length_from_utf8(input_data.data(), input_data.size()) + 512);
  size_t utf8count = simdutf::count_utf8(input_data.data(), input_data.size());
  // Let us warm up:
  bench(
      [&input_data, &buffer]() -> size_t {
        return simdutf::convert_utf8_to_utf16le(
            input_data.data(), input_data.size(), buffer.data());
      },
      2'000'000'000);
  // warmed up
  double best_speed = 0;
  double worst_speed = 1e300;

  for (size_t offset = 0; offset < 512; offset += 1) {

    std::cout << offset << ",\t";
    auto size_procedure = [&input_data, offset,
                           &buffer]() -> size_t {
      return simdutf::convert_utf8_to_utf16le(input_data.data(),
                                              input_data.size(), buffer.data() + offset);
    };
    double speed = utf8count / bench(size_procedure);
    if (speed > best_speed) {
      best_speed = speed;
    }
    if (speed < worst_speed) {
      worst_speed = speed;
    }

    std::cout << speed << "\n";
  }
  std::cout << "# worst speed " << worst_speed << std::endl;
  std::cout << "# best speed " << best_speed << std::endl;
  std::cout << "# difference (percent) " << (best_speed / worst_speed - 1) * 100
            << std::endl;
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
    run_from_utf8_output(input_data);
    std::cout << "----" << std::endl;

    run_from_utf8(input_data);
  } else {
    printf("We only support UTF-8 inputs.\n");
  }
  return EXIT_SUCCESS;
}
