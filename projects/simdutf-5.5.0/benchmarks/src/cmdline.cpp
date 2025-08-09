#include "cmdline.h"

#include <vector>
#include <stdexcept>

constexpr size_t DEFAULT_ITERATIONS = 30000; // Such a high number is helpful for stable measures.

namespace {

    using simdutf::benchmarks::CommandLine;

    CommandLine parse_arguments(int argc, char* argv[])
    {
        CommandLine cmdline;

        std::vector<std::string> arguments;
        for (int i=1; i < argc; i++) {
            std::string arg{argv[i]};
            if ((arg == "--help") || (arg == "-h")) {
                cmdline.show_help = true;
                return cmdline;
            }
            else if (arg == "--show-procedures") {
                cmdline.show_procedures = true;
            }
            else {
                arguments.push_back(std::move(arg));
            }
        }

        if (cmdline.show_procedures) {
            return cmdline;
        }

        for (size_t i=0; i < arguments.size(); /**/) {
            const std::string& arg = arguments[i];

            if ((arg == "-F") || (arg == "--input-file")) {
                const std::string& value = arguments.at(i + 1);
                cmdline.files.insert(value);
                i += 2;

                try {
                    while (true) {
                        const std::string& value = arguments.at(i);
                        if (! value.empty() && value[0] != '-') {
                            cmdline.files.insert(value);
                            i += 1;
                        } else
                            break;
                    }
                } catch (const std::out_of_range&) {
                    /* that's fine */;
                }
            }
            else if ((arg == "-P") || (arg == "--procedure")) {
                const std::string& value = arguments.at(i + 1);
                cmdline.procedures.insert(value);
                i += 2;
            }
            else if ((arg == "-I") || (arg == "--iterations")) {
                const std::string& value = arguments.at(i + 1);
                const long iterations = std::stoi(value);
                if (iterations <= 0) {
                    throw std::invalid_argument("Iterations must be greater than zero");
                }

                cmdline.iterations.insert(iterations);

                i += 2;
            }
            else if (arg == "--random-utf8") {
                const std::string& value = arguments.at(i + 1);
                const long size = std::stoi(value);
                if (size <= 0) {
                    throw std::invalid_argument("Input size must be greater than zero");
                }
                cmdline.random_size.insert(size);

                i += 2;
            }
            else
                throw std::invalid_argument("Unknown argument '" + arg + "'");
        } // for


        return cmdline;
    }

    void fixup_and_validate(CommandLine& cmdline) {
        if (cmdline.iterations.empty()) {
            const bool default_needed = (! cmdline.procedures.empty() ||
                                         ! cmdline.random_size.empty() ||
                                         ! cmdline.files.empty());
            if (default_needed)
                cmdline.iterations.insert(DEFAULT_ITERATIONS);
        }

        for (const auto& path: cmdline.files) {
            if (! std::filesystem::exists(path))
                throw std::runtime_error("File " + path.string() + " does not exist");
        }
    }

} // namespace

namespace simdutf::benchmarks {

CommandLine CommandLine::parse_arguments(int argc, char* argv[])
{
    CommandLine cmdline{::parse_arguments(argc, argv)};

    fixup_and_validate(cmdline);

    return cmdline;
}

bool CommandLine::empty() const {
    return procedures.empty()
       && random_size.empty()
       && files.empty()
       && iterations.empty()
       && (show_procedures == false);
}

void CommandLine::print_help() {
    print_help(stdout);
}

void CommandLine::print_help(FILE* file) {
    fputs(R"txt(
Benchmark utility for simdutf

Usage:

    -h, --help                      show help
    -F [PATH], --input-file [PATH]  set dataset path (may be used many times)
    -P [NAME], --procedure [NAME]   choose procedure(s) to test (may be used many times, a substring match suffices)
    -I --iterations                 number of iterations (default: 3000)
    --random-utf8 [size]            use random UTF8 data of given size
    --show-procedures               prints all known procedures for -P/--procedure

Examples:

    # test all known UTF8 procedures against 10k random input (in 100 iterations)
    $ benchmark --random-utf8 10240 -I 100

    # test procedures implemented with the haswell kernel against two custom files
    $ benchmark -P haswell -F ~/plain_ascii.txt -F ~/chinese_huge.txt


    # test two procedures (convert_utf8_to_utf16+llvm and ) implemented with the
    # haswell kernel against all files matching a pattern (POSIX)
    $ benchmark -P convert_utf8_to_utf16+llvm -P convert_utf8_to_utf16+u8u16 -F *.utf8.txt
)txt", file);
}

} // namespace simdutf::benchmarks
