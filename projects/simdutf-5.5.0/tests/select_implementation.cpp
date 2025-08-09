#include "simdutf.h"

#include <cstdlib>
#include <cstdio>
#include <string>

int main() {
  // This is just a demonstration, not actual testing required.
  std::string source = "La vie est belle.";
  std::string chosen_implementation;
  for (auto &implementation : simdutf::get_available_implementations()) {
    if (!implementation->supported_by_runtime_system()) {
      continue;
    }
    bool validutf8 = implementation->validate_utf8(source.c_str(), source.size());
    if (!validutf8) {
      return EXIT_FAILURE;
    }
    printf("%s: %s\n", implementation->name().c_str(), implementation->description().c_str());
    chosen_implementation = implementation->name();
  }
  auto my_implementation =
      simdutf::get_available_implementations()[chosen_implementation];
  if (!my_implementation) {
    return EXIT_FAILURE;
  }
  if (!my_implementation->supported_by_runtime_system()) {
    return EXIT_FAILURE;
  }
  simdutf::get_active_implementation() = my_implementation;
  bool validutf8 = simdutf::validate_utf8(source.c_str(), source.size());
  if (!validutf8) {
    return EXIT_FAILURE;
  }
  if (simdutf::get_active_implementation()->name() != chosen_implementation) {
    return EXIT_FAILURE;
  }
  printf("Manually selected: %s\n", simdutf::get_active_implementation()->name().c_str());
  return EXIT_SUCCESS;
}
