#pragma once

#include <set>
#include <vector>
#include <map>
#include <filesystem>
#include <variant>
#include <random>
#include "event_counter.h"
#include "simdutf.h"
namespace simdutf::benchmarks {

    namespace input {
        struct File {
            std::filesystem::path path;
        };

        struct random_utf8 {
            size_t size;
            int utf_1byte_prob = 1;
            int utf_2bytes_prob = 1;
            int utf_3bytes_prob = 1;
            int utf_4bytes_prob = 1;
        };

        struct Testcase {
            std::set<std::string> tested_procedures;
            size_t iterations;
            std::variant<File, random_utf8> input;
        };
    }

    class BenchmarkBase {
    protected:
        std::vector<uint8_t> input_data;
        std::vector<uint8_t> output_data;

        std::vector<input::Testcase> testcases;

        uint32_t seed{1234};

    public:
        BenchmarkBase(std::vector<input::Testcase>&& testcases);
        bool run();
        virtual const std::set<std::string>& all_procedures() const = 0;
        virtual std::set<simdutf::encoding_type> expected_encodings(const std::string& procedure) = 0;

    protected:
        virtual void run(const std::string& procedure_name, size_t iterations) = 0;

        template<typename PROCEDURE>
        event_aggregate count_events(PROCEDURE, size_t iterations);
        void print_summary(const event_aggregate& all, double data_size, double character_count) const;
        void print_summary(const event_aggregate& all, size_t data_size, size_t character_count) const;

        void run(const input::Testcase& testcase);
        void run(const std::string& procedure_name, const input::Testcase& testcase);
        void prepare_input(const input::Testcase& benchmark);
        void load_file(const std::filesystem::path& path);
    };

    template<typename PROCEDURE>
    event_aggregate BenchmarkBase::count_events(PROCEDURE procedure, size_t iterations) {
        event_collector collector;
        event_aggregate all{};
        // Some inputs are just too small to measure accurately, so we need to scale them up.
        size_t multiplier = 1;
        while(true) {
          event_aggregate test{};
          collector.start();
          for(size_t i = 0; i < multiplier; i++) { procedure(); }
          event_count allocate_count = collector.end();
          test << allocate_count;
          if(test.best.elapsed_ns() < 4000) {
            multiplier *= 2;
          } else {
            break;
          }
        }
        for(size_t i = 0; i < iterations; i++) {
          collector.start();
          for(size_t i = 0; i < multiplier; i++) { procedure(); }
          event_count allocate_count = collector.end();
          all << allocate_count / multiplier;
        }
        all.has_events = collector.has_events();
        return all;
    }
}
