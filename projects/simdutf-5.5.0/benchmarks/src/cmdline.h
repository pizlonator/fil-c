#pragma once

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <string>
#include <set>

namespace simdutf::benchmarks {

    class CommandLine
    {
    public:
        bool show_help = false;
        bool show_procedures = false;
        std::set<std::string> procedures;
        std::set<size_t> random_size;
        std::set<std::filesystem::path> files;
        std::set<size_t> iterations;

    public:
        CommandLine() = default;
        static CommandLine parse_arguments(int argc, char* argv[]);

        bool empty() const;
        static void print_help(FILE*);
        static void print_help();
    };
}
