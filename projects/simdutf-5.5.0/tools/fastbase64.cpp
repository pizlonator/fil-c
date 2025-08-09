#include "simdutf.h"

#include <array>
#include <cerrno>
#include <filesystem>

class CommandLine {
public:
  std::FILE *current_file{NULL};
  std::filesystem::path output_file;

  CommandLine() = default;
  static CommandLine parse_and_validate_arguments(int argc, char *argv[]);
  bool run_procedure(std::FILE *fpout);
  bool encode_to(std::FILE *fpout);
  bool decode_to(std::FILE *fpout);
  bool run();
  std::pair<bool, size_t> load_chunk(char *input_data, size_t chunk_size,
                                     size_t offset);
  bool write_to_file_descriptor(std::FILE *fp, const char *data, size_t length);
  static void show_help();
  bool decode;
};

CommandLine CommandLine::parse_and_validate_arguments(int argc, char *argv[]) {
  CommandLine cmdline;
  std::vector<std::string> arguments;

  for (int i = 1; i < argc; i++) {
    std::string arg{argv[i]};
    if ((arg == "-h") || (arg == "--help")) {
      CommandLine::show_help();
      return cmdline;
    } else {
      arguments.push_back(std::move(arg));
    }
  }
  if (arguments.size() == 0) {
    throw std::runtime_error("Too few arguments!");
  }
  cmdline.decode = false;
  for (std::string &a : arguments) {
    if (a == "-d") {
      cmdline.decode = true;
    } else if (a == "-e") {
      cmdline.decode = false;
    } else if (a[0] == '-') {
      throw std::runtime_error("Unknown option: " + a);
    } else {
      if (cmdline.current_file == NULL) {
        cmdline.current_file = std::fopen(a.c_str(), "rb");
        if (cmdline.current_file == NULL) {
          throw std::runtime_error("Could not open file: " + a + ":" +
                                   std::string(strerror(errno)));
        }
      } else if (cmdline.output_file.empty()) {
        cmdline.output_file = a;
      } else {
        throw std::runtime_error("Too many arguments!");
      }
    }
  }
  return cmdline;
}

bool CommandLine::run() {
  if (current_file == NULL) {
    throw std::runtime_error("No input file specified!");
  }
  if (output_file.empty()) {
    return run_procedure(stdout);
  } else {
    SIMDUTF_PUSH_DISABLE_WARNINGS
    SIMDUTF_DISABLE_DEPRECATED_WARNING
    std::FILE *fp = std::fopen(output_file.string().c_str(), "wb");
    SIMDUTF_POP_DISABLE_WARNINGS
    if (fp == NULL) {
      fprintf(stderr, "Could not open %s: %s\n", output_file.string().c_str(), strerror(errno));
      return false;
    }
    bool success = run_procedure(fp);
    // Let us first try to close the file.
    if (fclose(fp) != 0) {
      fprintf(stderr, "Failed to close %s: %s\n", output_file.string().c_str(), strerror(errno));
      return false;
    }
    return success;
  }
}

bool CommandLine::run_procedure(std::FILE *fpout) {
  if (decode) {
    return decode_to(fpout);
  } else {
    return encode_to(fpout);
  }
}

// load_chunk returns a pair of a boolean and a size_t, the boolean is true
// until we reach the end of the stream, the size_t is the number of bytes read.
std::pair<bool, size_t>
CommandLine::load_chunk(char *input_data, size_t chunk_size, size_t offset) {
  size_t bytes_read =
      std::fread(input_data + offset, 1, chunk_size - offset, current_file);
  if (std::ferror(current_file)) {
    std::fclose(current_file);
    throw std::runtime_error("Error while reading:" +
                             std::string(strerror(errno)));
  }
  if (std::feof(current_file)) { // Check if current_file is done
    std::fclose(current_file);   // best effort
    current_file = NULL;
    return {false, bytes_read};
  }
  return {true, bytes_read};
}

bool CommandLine::write_to_file_descriptor(std::FILE *fp, const char *data,
                                           size_t length) {
  if (fp == NULL) {
    return false;
  }
  size_t bytes_written = std::fwrite(data, 1, length, fp);
  if (bytes_written != length) {
    throw std::runtime_error("Failed to write:" + std::string(strerror(errno)));
  }
  return true;
}

bool CommandLine::decode_to(std::FILE *fpout) {
  const size_t chunk_size = 65536;
  std::array<char, chunk_size> input_data;
  std::array<char, (chunk_size + 3) / 4 * 3> output_buffer;
  size_t pos =
      0; // the pos variable keeps track of the position in the input file.
  // Its purpose is to provide a position for error messages.
  size_t offset = 0;
  // load_chunk returns a pair of a boolean and a size_t, the boolean is true
  // until we reach the end of the stream, the size_t is the number of bytes
  // read.
  for (auto p = load_chunk(input_data.data(), chunk_size, offset); p.second > 0;
       p = load_chunk(input_data.data(), chunk_size, offset)) {
    // We convertto base64 the data we have read so far
    simdutf::result r = simdutf::base64_to_binary(
        input_data.data(), p.second + offset, output_buffer.data());
    // If we have encountered an invalid character, we print an error message
    // and return false.
    if (r.error == simdutf::error_code::INVALID_BASE64_CHARACTER) {
      fprintf(stderr, "Invalid base64 character at position %zu\n.",
              pos + r.count);
      return false;
    }
    // if p.first is false, we have reached the end of the file.
    if (!p.first) {
      // At the end of the file, if we are left with one base64 character
      // leftover, it is a fatal error.
      if (r.error == simdutf::error_code::BASE64_INPUT_REMAINDER) {
        fprintf(stderr, "The base64 input contained an invalid number of "
                        "characters or could not be read.");
        return false;
      }
      // Otherwise, we write the output and return true.
      write_to_file_descriptor(fpout, output_buffer.data(), r.count);
      return true;
    }
    // We want to write the data in chunks of 3 bytes and read blocks of
    // 4 bytes. We keep the last 0, 1, 3 or 4 base64 bytes in the input buffer.
    // And we write the output in chunks of 3 bytes.
    offset = 0;
    if (r.error == simdutf::error_code::BASE64_INPUT_REMAINDER) {
      offset = 1;
    } else {
      offset = (r.count % 3) == 0 ? 0 : (r.count % 3) + 1;
    }
    // Copy 0, 1, 2 or 3 non-space bytes to the input buffer.
    size_t copied = 0;
    for (size_t z = p.second - 1; copied < offset; z--) {
      if (input_data[z] == ' ' || input_data[z] == '\n' ||
          input_data[z] == '\r' || input_data[z] == '\t') {
        continue;
      }
      copied++;
      input_data[offset - copied] = input_data[z];
    }
    // We write a multiple of 3 bytes to the output buffer, discarding
    // the last 0, 1 or 2 bytes.
    r.count -= (r.count % 3);
    write_to_file_descriptor(fpout, output_buffer.data(), r.count);
    pos += p.second;
  }
  return true;
}

bool CommandLine::encode_to(std::FILE *fpout) {
  const size_t chunk_size = 49152;
  std::array<char, chunk_size> input_data;
  std::array<char, (chunk_size + 2) / 3 * 4> output_buffer;
  size_t pos = 0;
  size_t offset = 0;
  // load_chunk returns a pair of a boolean and a size_t, the boolean is true
  // until we reach the end of the stream, the size_t is the number of bytes
  // read.
  for (auto p = load_chunk(input_data.data(), chunk_size, offset); p.second > 0;
       p = load_chunk(input_data.data(), chunk_size, offset)) {
    // We have read p.second bytes, and we have offset bytes left from the
    // previous chunk.
    size_t total_bytes = p.second + offset;
    // If we have reached the end of the file, we write the output and return
    // true.
    if (!p.first) {
      // We finish the file
      size_t output_size = simdutf::binary_to_base64(
          input_data.data(), total_bytes, output_buffer.data());
      write_to_file_descriptor(fpout, output_buffer.data(), output_size);
      return true;
    }
    // We want to write the data in chunks of 4 bytes and read blocks of
    // 3 bytes. We keep the last 0, 1 or 2 bytes in the input buffer.
    offset = total_bytes % 3;
    total_bytes -= offset;
    size_t output_size = simdutf::binary_to_base64(
        input_data.data(), total_bytes, output_buffer.data());
    write_to_file_descriptor(fpout, output_buffer.data(), output_size);
    // Copy 0, 1 or 2 bytes to the start of the input buffer.
    memcpy(input_data.data(), input_data.data() + total_bytes, offset);
  }
  return true;
}

void CommandLine::show_help() {
  printf("Usage: fastbase64 [OPTIONS...] [INPUTFILE] [OUTPUTFILE]\n\n");
  printf("  -d       decode base64\n"
         "  -e       encode base64 (default)\n\n");
  printf("If output is not specified, the output is redirected to standard "
         "output.\n");
}

int main(int argc, char *argv[]) {
  try {
    CommandLine cmdline = CommandLine::parse_and_validate_arguments(argc, argv);
    return cmdline.run() ? EXIT_SUCCESS : EXIT_FAILURE;
  } catch (const std::exception &e) {
    fprintf(stderr, "%s\n", e.what());
    CommandLine::show_help();
    return EXIT_FAILURE;
  }
}