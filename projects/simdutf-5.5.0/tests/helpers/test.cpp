#include "test.h"

#include <set>
#include <vector>
#include <stdexcept>
#include <cstdio>


namespace {

  struct CommandLine {
    bool show_help{false};
    bool show_tests{false};
    bool show_architectures{false};
    std::set<std::string> architectures;
    std::vector<std::string> tests;
  };

  CommandLine parse(int argc, char* argv[]) {
    CommandLine cmdline;

    std::list<std::string> args;
    for (int i=1; i < argc; i++) {
      std::string arg{argv[i]};
      if ((arg == "-h") || (arg == "--help")) {
        cmdline.show_help = true;
        return cmdline;
      }

      if (arg == "--show-architectures") {
        cmdline.show_architectures = true;
        continue;
      }

      if (arg == "--show-tests") {
        cmdline.show_tests = true;
        continue;
      }

      args.push_back(std::move(arg));
    }

    if (cmdline.show_tests or cmdline.show_architectures) {
      return cmdline;
    }

    while (not args.empty()) {
      const auto arg = args.front();
      args.pop_front();

      if ((arg == "-a") or (arg == "--arch")) {
        if (args.empty()) {
          throw std::invalid_argument("Expected architecture name after " + arg);
        }

        cmdline.architectures.insert(args.front());
        args.pop_front();
      } else if ((arg == "-t") or (arg == "--test")) {
        if (args.empty()) {
          throw std::invalid_argument("Expected test name after " + arg);
        }

        cmdline.tests.push_back(args.front());
        args.pop_front();
      } else {
        throw std::invalid_argument("Unknown argument '" + arg + "'");
      }
    }

    return cmdline;
  }

  void print_help(FILE* file) {
    fputs(R"txt(
Test utility for simdutf

Usage:

    -h, --help                      show help
    --show-architectures            show available architectures
    --show-tests                    show name of available tests
    -a [ARCH], --arch [ARCH]        run tests only for selected architecture(s)
    -t [TEST], --test [TEST]        run tests matching all given strings

Examples:

    # Test haswell implementations with tests having strings 'convert'
    # and 'utf8' in their name -- for instance 'convert_utf8_to_utf16' and
    # 'convert_utf16_to_utf8'.

    $ test --arch haswell --test convert --test utf8
)txt", file);
  }

  void print_help() {
    print_help(stdout);
  }

  void print_architectures(FILE *file) {
#if SIMDUTF_IS_BIG_ENDIAN
    fprintf(file, "Big-endian system detected.\n");
#else
    fprintf(file, "Little-endian system detected.\n");
#endif
    fprintf(file, "Available implementations:\n");
    for (const auto& implementation: simdutf::get_available_implementations()) {
      if (implementation == nullptr) {
        puts("implementation is null which is unexpected.");
        abort();
      }
      if (implementation->supported_by_runtime_system()) {
        fprintf(file, "- %s\n", implementation->name().c_str());
      } else {
        fprintf(file, "- %s [unsupported by current processor]\n", implementation->name().c_str());
      }
    }
  }

  void print_architectures() {
    print_architectures(stdout);
  }

  void print_tests(FILE *file) {
    fprintf(file, "Available tests:\n");
    for (const auto& test: simdutf::test::test_procedures()) {
      fprintf(file, "- %s\n", test.name.c_str());
    }
  }

  void print_tests() {
    print_tests(stdout);
  }

  void run(const CommandLine& cmdline) {
    if (cmdline.show_help) {
      print_help();
      return;
    }

    if (cmdline.show_architectures) {
      print_architectures();
      return;
    }

    if (cmdline.show_tests) {
      print_tests();
      return;
    }
    size_t matching_implementation{0};

    for (const auto& implementation: simdutf::get_available_implementations()) {
      if (implementation == nullptr) {
        puts("implementation is null which is unexpected");
        abort();
      }
      if (!implementation->supported_by_runtime_system()) {
        printf("Implementation %s is unsupported by the current processor.\n", implementation->name().c_str());
        continue;
      }
      if (not cmdline.architectures.empty()) {
          if (cmdline.architectures.count(implementation->name()) == 0) {
            continue;
          }
      }
      matching_implementation++;

      printf("Checking implementation %s\n", implementation->name().c_str());

      auto filter = [&cmdline](const simdutf::test::test_entry& test) -> bool {
        if (cmdline.tests.empty())
          return true;

        for (const auto& str: cmdline.tests) {
          if (test.name.find(str) != std::string::npos)
            return true;
        }

        return false;
      };

      for (auto test: simdutf::test::test_procedures()) {
        if (filter(test)) { test(*implementation); }
      }
    }
    if(matching_implementation == 0) {
      puts("not a single compatible implementation found, this is an error");
      abort();
    }
  }

} // namespace

namespace simdutf { namespace test {

  std::list<test_entry>& test_procedures() {
    static std::list<test_entry> singleton;

    return singleton;
  }

  register_test::register_test(const char* name, test_procedure proc) {
    test_procedures().push_back({name, proc});
  }

  int main(int argc, char* argv[]) {
    const auto cmdline = parse(argc, argv);

    run(cmdline);

    return 0;
  }

}} // namespace namespace simdutf::test
