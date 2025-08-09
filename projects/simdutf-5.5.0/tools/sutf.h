#include "simdutf.h"

#if !defined(ICONV_AVAILABLE) && __has_include (<iconv.h>)
#define ICONV_AVAILABLE 1
#endif //__has_include (<iconv.h>)
#if ICONV_AVAILABLE
#include <iconv.h>
#endif

#include <filesystem>
#include <array>
#include <memory>
#include <queue>

constexpr size_t CHUNK_SIZE = 65536;    // Must be at least 4

class CommandLine
{
public:
  std::string from_encoding;
  std::string to_encoding;
  std::queue<std::filesystem::path> input_files;
  std::FILE* current_file{NULL};
  std::filesystem::path output_file;
  std::array<uint8_t, CHUNK_SIZE> input_data;
  std::array<char, CHUNK_SIZE*sizeof(uint32_t)> output_buffer;

  CommandLine() = default;
  static CommandLine parse_and_validate_arguments(int argc, char* argv[]);
  static void show_help();
  static void show_usage();
  static void show_formats();

  void run();
  void run_procedure(std::FILE *fp);
  template <typename PROCEDURE>
  void run_simdutf_procedure(PROCEDURE proc);
  void iconv_fallback(std::FILE *fp);
  bool load_chunk(size_t *input_size);
  bool write_to_file_descriptor(std::FILE *fp, const char * data, size_t length);
  size_t find_last_leading_byte(size_t size);
};