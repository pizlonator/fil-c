#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

/*
 * this provides a way to run garbage data through the library also on platforms
 * that do not have libFuzzer. data can be taken from a fuzzing session
 * and feeding it to a program with this main function.
 */

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size);

void handle_file(std::filesystem::path file) {
  std::vector<char> data;

  const auto Nbytes = file_size(file);
  data.resize(Nbytes);

  std::ifstream filestream(file);
  filestream.read(data.data(), data.size());
  if (filestream.gcount() != Nbytes) {
    std::cerr << "failed reading from file " << file << '\n';
    return;
  }

  std::cout << "invoking fuzzer on data from file " << file << '\n';

  LLVMFuzzerTestOneInput(reinterpret_cast<const uint8_t*>(data.data()),
                         data.size());
}

void handle_directory(std::filesystem::path directory) {
  for (const auto& entry : std::filesystem::recursive_directory_iterator(
           directory,
           std::filesystem::directory_options::follow_directory_symlink)) {
    if (entry.is_regular_file()) {
      handle_file(entry.path());
    } else if (entry.is_symlink()) {
      auto resolved = read_symlink(entry);
      if (is_regular_file(resolved)) {
        handle_file(resolved);
      }
    }
  }
}

void handle_possible_file(std::filesystem::path possible_file) {
  if (std::filesystem::is_directory(possible_file)) {
    handle_directory(possible_file);
  } else if (is_regular_file(possible_file)) {
    handle_file(possible_file);
  } else if (is_symlink(possible_file)) {
    auto resolved = read_symlink(possible_file);
    handle_possible_file(resolved);
  } else {
    std::cerr << "not a directory, regular file or symlink: " << possible_file
              << '\n';
  }
}

void handle_arg(const char* path) {
  return handle_possible_file(std::filesystem::path(path));
}

int main(int argc, char* argv[]) {
  for (int i = 1; i < argc; ++i) {
    handle_arg(argv[i]);
  }
}
