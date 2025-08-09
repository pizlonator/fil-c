#include "src/cmdline.h"
#include "src/benchmark.h"

#include <iostream>

void info_message() {
    std::cout << "We define the number of bytes to be the number of *input* bytes.\n";
    std::cout << "We define a 'char' to be a code point (between 1 and 4 bytes).\n";
    std::cout << "===========================\n";
}

int main(int argc, char* argv[]) {
    info_message();
#ifdef INOUE2008
    inoue2008::inoue_test(); // minimal testing
#endif
    using simdutf::benchmarks::CommandLine;
    CommandLine cmdline;
    try {
        cmdline = CommandLine::parse_arguments(argc, argv);
        if (cmdline.show_help) {
            CommandLine::print_help();
            return EXIT_SUCCESS;
        }
        else if (cmdline.empty()) {
            CommandLine::print_help();
            return EXIT_FAILURE;
        }
    } catch (const std::exception& e) {
        printf("%s\n", e.what());
        return EXIT_FAILURE;
    }

    using simdutf::benchmarks::Benchmark;

    Benchmark benchmark{Benchmark::create(cmdline)};
    if (cmdline.show_procedures) {
        const auto& known_procedures = benchmark.all_procedures();
        printf("Available procedures (%zu)\n", size_t(known_procedures.size()));
        for (const auto& name: known_procedures) {
            printf("- %s\n", name.c_str());
        }
        return EXIT_SUCCESS;
    } else {
        return benchmark.run() ? EXIT_SUCCESS : EXIT_FAILURE;
    }
}
