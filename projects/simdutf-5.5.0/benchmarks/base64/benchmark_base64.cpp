
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>

#include "libbase64.h"
#include "libbase64_spaces.h"
#include "node_base64.h"
#include "openssl3_base64.h"

#include "simdutf.h"

#include "event_counter.h"
#include <atomic>

bool is_space(char c) {
  static const bool is_space[256] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  return is_space[uint8_t(c)];
}
// This is for reference only, do not use this function in production
// system.
int base64_decode_skip_spaces(const char *src, size_t srclen, char *out,
                              size_t *outlen) {
  struct base64_state state;
  base64_stream_decode_init(&state, 0);
  const char *srcend = src + srclen;
  char *initout = out;
  // If the input starts with spaces, we skip them.
  while (src < srcend && is_space(*src)) {
    src++;
  }
  while (src < srcend) {
    size_t len = 0;
    const char *srcendtoken = src;
    while (srcendtoken < srcend && !is_space(*srcendtoken)) {
      srcendtoken++;
    }
    int r = base64_stream_decode(&state, src, size_t(srcendtoken - src),
                                 (char *)out, &len);
    if (r != 1) {
      return -1; // fatal error.
    }
    out += len;
    src = srcendtoken;
    while (src < srcend && is_space(*src)) {
      src++;
    }
  }

  *outlen = size_t(out - initout);
  return !state.bytes;
}

enum : uint8_t { roundtrip = 0, decode = 1, encode = 2, bun = 3, roundtripurl = 4 };

event_collector collector;

template <class function_type>
event_aggregate bench(const function_type &function, size_t min_repeat = 10,
                      size_t min_time_ns = 1000000000,
                      size_t max_repeat = 1000000) {
  event_aggregate aggregate{};
  size_t N = min_repeat;
  if (N == 0) {
    N = 1;
  }
  for (size_t i = 0; i < N; i++) {
    std::atomic_thread_fence(std::memory_order_acquire);
    collector.start();
    function();
    std::atomic_thread_fence(std::memory_order_release);
    event_count allocate_count = collector.end();
    aggregate << allocate_count;
    if ((i + 1 == N) && (aggregate.total_elapsed_ns() < min_time_ns) &&
        (N < max_repeat)) {
      N *= 10;
    }
  }
  return aggregate;
}

std::vector<char> read_file(const char *filename,
                            bool trim_final_newline = true) {
  std::ifstream file;
  file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  file.open(filename);
  std::vector<char> input_data;
  input_data.assign(std::istreambuf_iterator<char>(file),
                    std::istreambuf_iterator<char>());
  if (trim_final_newline && !input_data.empty() && input_data.back() == '\n') {
    input_data.pop_back();
  }
  return input_data;
}

void show_help() {
  printf("Usage: benchmark_base64 [options] file1 [file2 ...]\n");
  printf("Options:\n");
  printf("  -h, --help         Show this help message and exit\n");
  printf("  -d, --decode       Decode the input file\n");
  printf("  -e, --encode       Encode the input file\n");
  printf("  -r, --roundtrip    Roundtrip the input file\n");
  printf("  --roundtrip-url    Roundtrip the input file (URL)\n");
  printf("  -b, --bench-bun    Bun benchmark\n");

  printf(" See https://github.com/lemire/base64data for test data.\n");
}
void pretty_print(size_t, size_t bytes, std::string name, event_aggregate agg) {
  printf("%-40s : ", name.c_str());
  printf(" %5.2f GB/s ", bytes / agg.elapsed_ns());
  if (collector.has_events()) {
    printf(" %5.2f GHz ", agg.cycles() / agg.elapsed_ns());
    printf(" %5.2f c/b ", agg.cycles() / bytes);
    printf(" %5.2f i/b ", agg.instructions() / bytes);
    printf(" %5.2f i/c ", agg.instructions() / agg.cycles());
  }
  printf("\n");
}
bool can_decode_all(std::vector<std::vector<char>> &data) {
  for (const std::vector<char> &source : data) {
    std::vector<char> buffer(simdutf::maximal_binary_length_from_base64(
        source.data(), source.size()));

    auto err =
        simdutf::base64_to_binary(source.data(), source.size(), buffer.data());
    if (err.error) {
      return false;
    }
  }
  return true;
}

std::vector<std::vector<char>> split(const std::vector<char> &data) {
  std::vector<std::vector<char>> answer;
  const char *p = data.data();
  const char *end = p + data.size();
  while (p < end) {
    const char *q = p;
    while ((q < end) && (*q != '\n') && (*q != '\r')) {
      q++;
    }
    answer.emplace_back(p, q);
    p = q;
    if (p < end) {
      p++;
    }
    while (p < end && ((*p == '\n') || (*p == '\r'))) {
      p++;
    }
  }
  return answer;
}

void check_for_single_line(std::vector<std::vector<char>> &data) {
  if (can_decode_all(data)) {
    return;
  }
  printf("# I cannot decode all the base64 data as single line, so I will try "
         "to split the data.\n");
  std::vector<std::vector<char>> newdata;
  for (const std::vector<char> &source : data) {
    auto splits = split(source);
    for (auto &split : splits) {
      newdata.push_back(split);
    }
  }
  printf("# Turned %zu inputs into %zu inputs.\n", data.size(), newdata.size());

  data = std::move(newdata);
}

bool contains_spaces(std::vector<std::vector<char>> &data) {
  for (const std::vector<char> &source : data) {
    for (char c : source) {
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        return true;
      }
    }
  }
  return false;
}

void bench(std::vector<std::vector<char>> &data, uint8_t mode) {
  size_t volume = std::accumulate(
      data.begin(), data.end(), size_t(0),
      [](size_t a, const std::vector<char> &b) { return a + b.size(); });
  size_t max_size = std::max_element(data.begin(), data.end(),
                                     [](const std::vector<char> &a,
                                        const std::vector<char> &b) {
                                       return a.size() < b.size();
                                     })
                        ->size();
  std::vector<char> buffer1(simdutf::base64_length_from_binary(max_size));
  std::vector<char> buffer2(max_size);
  printf("# volume: %zu bytes\n", volume);
  printf("# max length: %zu bytes\n", max_size);
  printf("# number of inputs: %zu\n", data.size());

  switch (mode) {

  case roundtripurl: {
    printf("# roundtrip (url)\n");
    for (auto &e : simdutf::get_available_implementations()) {
      if (!e->supported_by_runtime_system()) {
        continue;
      }
      pretty_print(data.size(), volume, "simdutf::" + e->name(),
                   bench([&data, &buffer1, &buffer2, &e]() {
                     for (const std::vector<char> &source : data) {
                       size_t base64_size = e->binary_to_base64(
                           source.data(), source.size(), buffer1.data(), simdutf::base64_url);
                       auto err = e->base64_to_binary(
                           buffer1.data(), base64_size, buffer2.data(), simdutf::base64_url);
                       if (err.error) {
                         std::cerr << "Error:  at position " << err.count
                                   << std::endl;
                       } else if (err.count != source.size()) {
                         std::cerr << "Error: " << err.count
                                   << " bytes decoded, expected "
                                   << source.size() << std::endl;
                       }
                     }
                   }));
    }
    break;
  }
  case roundtrip: {
    printf("# roundtrip\n");
    pretty_print(
        data.size(), volume, "libbase64", bench([&data, &buffer1, &buffer2]() {
          for (const std::vector<char> &source : data) {

            size_t outlen;

            base64_encode(source.data(), source.size(), buffer1.data(), &outlen,
                          0);
            int result = base64_decode(buffer1.data(), outlen, buffer2.data(),
                                       &outlen, 0);
            if (result != 1) {
              std::cerr << "Error: " << result << " failed to decode base64 "
                        << std::endl;
            }
            if (outlen != source.size()) {
              std::cerr << "Error: " << result << " bytes decoded, expected "
                        << source.size() << std::endl;
            }
          }
        }));
    for (auto &e : simdutf::get_available_implementations()) {
      if (!e->supported_by_runtime_system()) {
        continue;
      }
      pretty_print(data.size(), volume, "simdutf::" + e->name(),
                   bench([&data, &buffer1, &buffer2, &e]() {
                     for (const std::vector<char> &source : data) {
                       size_t base64_size = e->binary_to_base64(
                           source.data(), source.size(), buffer1.data());
                       auto err = e->base64_to_binary(
                           buffer1.data(), base64_size, buffer2.data());
                       if (err.error) {
                         std::cerr << "Error:  at position " << err.count
                                   << std::endl;
                       } else if (err.count != source.size()) {
                         std::cerr << "Error: " << err.count
                                   << " bytes decoded, expected "
                                   << source.size() << std::endl;
                       }
                     }
                   }));
    }
    break;
  }
  case decode: {
    printf("# decode\n");
    pretty_print(
        data.size(), volume, "memcpy", bench([&data, &buffer1, &buffer2]() {
          for (const std::vector<char> &source : data) {
            memcpy(buffer1.data(), source.data(), source.size());
          }
        }));
    bool spaces = contains_spaces(data);
    if (spaces) {
      printf("# the base64 data contains spaces, so we cannot use straigth "
             "libbase64::base64_decode directly\n");
    } else {
      pretty_print(data.size(), volume, "libbase64",
                   bench([&data, &buffer1, &buffer2]() {
                     for (const std::vector<char> &source : data) {

                       size_t outlen;
                       int result = base64_decode(source.data(), source.size(),
                                                  buffer1.data(), &outlen, 0);
                       if (result != 1) {
                         std::cerr << "Error: " << result
                                   << " failed to decode base64 " << std::endl;
                         throw std::runtime_error(
                             "Error: failed to decode base64 ");
                       }
                     }
                   }));
    }
    pretty_print(
        data.size(), volume, "libbase64_space_decode",
        bench([&data, &buffer1, &buffer2]() {
          for (const std::vector<char> &source : data) {

            size_t outlen;
            bool ok = libbase64_space_decode(source.data(), source.size(),
                                             buffer1.data(), &outlen);
            if (!ok) {
              std::cerr << "Error: "
                        << " failed to decode base64 " << std::endl;
              throw std::runtime_error("Error: failed to decode base64 ");
            }
          }
        }));
    pretty_print(
        data.size(), volume, "openssl3.3.x", bench([&data, &buffer1, &buffer2]() {
          for (const std::vector<char> &source : data) {
            int result = openssl3::base64_decode(buffer1.data(), buffer1.size(),
                                             source.data(), source.size());
            (void)result;
          }
        }));
    pretty_print(
        data.size(), volume, "node", bench([&data, &buffer1, &buffer2]() {
          for (const std::vector<char> &source : data) {
            int result = node::base64_decode(buffer1.data(), buffer1.size(),
                                             source.data(), source.size());
            (void)result;
          }
        }));
    for (auto &e : simdutf::get_available_implementations()) {
      if (!e->supported_by_runtime_system()) {
        continue;
      }
      pretty_print(
          data.size(), volume, "simdutf::" + e->name(),
          bench([&data, &buffer1, &buffer2, &e]() {
            for (const std::vector<char> &source : data) {
              auto err = e->base64_to_binary(source.data(), source.size(),
                                             buffer1.data());
              if (err.error) {
                std::cerr << "Error: at position " << err.count << " out of "
                          << source.size() << std::endl;
                for (size_t i = err.count; i < source.size(); i++) {
                  printf("0x%02x (%c) ", uint8_t(source[i]), source[i]);
                }
                printf("\n");
                throw std::runtime_error("Error: is input valid base64? " +
                                         std::to_string(err.error) +
                                         " at position " +
                                         std::to_string(err.count));
              }
            }
          }));
    }
    break;
  }
  case encode: {
    printf("# encode\n");
    pretty_print(
        data.size(), volume, "memcpy", bench([&data, &buffer1, &buffer2]() {
          for (const std::vector<char> &source : data) {
            memcpy(buffer1.data(), source.data(), source.size());
          }
        }));
    volatile size_t base64_size;
    pretty_print(data.size(), volume, "libbase64",
                 bench([&data, &buffer1, &base64_size]() {
                   for (const std::vector<char> &source : data) {
                     size_t outlen;
                     base64_encode(source.data(), source.size(), buffer1.data(),
                                   &outlen, 0);
                     base64_size = outlen;
                   }
                 }));
    for (auto &e : simdutf::get_available_implementations()) {
      if (!e->supported_by_runtime_system()) {
        continue;
      }
      pretty_print(data.size(), volume, "simdutf::" + e->name(),
                   bench([&data, &buffer1, &e, &base64_size]() {
                     for (const std::vector<char> &source : data) {
                       base64_size = e->binary_to_base64(
                           source.data(), source.size(), buffer1.data());
                     }
                   }));
    }
    break;
  }
  }
}

int bench_bun() {
  /**
   * See
   * https://github.com/oven-sh/bun/blob/main/bench/snippets/buffer-to-string.mjs
   *
   * const bigBuffer = Buffer.from("hello world".repeat(10000));
   * const converted = bigBuffer.toString("base64");
   * const uuid = crypto.randomBytes(16);
   *
   * bench(`Buffer(${bigBuffer.byteLength}).toString('base64')`, () => {
   * return bigBuffer.toString("base64");
   * });
   *
   * bench(`Buffer(${uuid.byteLength}).toString('base64')`, () => {
   *  return uuid.toString("base64");
   * });
   */
  printf("# benching bun (essentially an encoding bench)\n");
  std::string bigBuffer = "hello world";
  bigBuffer.reserve(10000 * bigBuffer.size());
  for (size_t i = 1; i < 10000; i++) {
    bigBuffer += "hello world";
  }
  std::string crypto;
  for (size_t i = 0; i < 16; i++) {
    crypto += rand();
  }
  std::vector<std::pair<std::string, std::string>> tests = {
      {"big hello world", bigBuffer}, {"random 16 bytes", crypto}};
  // Could be nicer with C++20
  for (auto &i : tests) {
    printf("# %s\n", i.first.c_str());
    std::string source = i.second;
    volatile size_t base64_size;
    std::vector<char> buffer1(
        simdutf::base64_length_from_binary(source.size()));
    pretty_print(1, source.size(), "libbase64",
                 bench([&source, &buffer1, &base64_size]() {
                   size_t outlen;
                   base64_encode(source.data(), source.size(), buffer1.data(),
                                 &outlen, 0);
                   base64_size = outlen;
                 }));
    for (auto &e : simdutf::get_available_implementations()) {
      if (!e->supported_by_runtime_system()) {
        continue;
      }
      pretty_print(1, source.size(), "simdutf::" + e->name(),
                   bench([&source, &buffer1, &e, &base64_size]() {
                     base64_size = e->binary_to_base64(
                         source.data(), source.size(), buffer1.data());
                   }));
    }
  }
  return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
  printf("# current system detected as %s.\n",
         simdutf::get_active_implementation()->name().c_str());
  if (argc < 2) {
    show_help();
    return EXIT_FAILURE;
  }

  std::vector<std::string> arguments;
  auto mode = roundtrip;

  for (int i = 1; i < argc; i++) {
    std::string arg{argv[i]};
    if ((arg == "-h") || (arg == "--help")) {
      show_help();
      return EXIT_SUCCESS;
    } else if ((arg == "-d") || (arg == "--decode")) {
      mode = decode;
    } else if ((arg == "-e") || (arg == "--encode")) {
      mode = encode;
    } else if ((arg == "-r") || (arg == "--roundtrip")) {
      mode = roundtrip;
    } else if (arg == "--roundtrip-url") {
      mode = roundtripurl;
    } else if ((arg == "-b") || (arg == "--bun")) {
      mode = bun;
    } else {
      arguments.push_back(std::move(arg));
    }
  }
  if (mode == bun) {
    return bench_bun();
  }
  auto return_value = EXIT_SUCCESS;
  std::vector<std::vector<char>> input;
  printf("# loading files: ");
  for (auto &arg : arguments) {
    try {
      printf("."); // show progress
      fflush(stdout);
      input.push_back(read_file(arg.c_str(), mode == decode));
    } catch (const std::exception &e) {
      printf("\n# tried loading file: %s\n", arg.c_str());
      std::cerr << "Error: " << e.what() << std::endl;
      return EXIT_FAILURE;
    }
  }
  printf("\n");
  if (mode == decode) {
    check_for_single_line(input);
  }
  try {
    bench(input, mode);
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return_value = EXIT_FAILURE;
  }

  return return_value;
}
