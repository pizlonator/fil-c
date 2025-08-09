#ifndef __EVENT_COUNTER_H
#define __EVENT_COUNTER_H

#include <cctype>
#ifndef _MSC_VER
#include <dirent.h>
#endif
#include <cinttypes>

#include <cstring>

#include <chrono>
#include <vector>

#include "linux-perf-events.h"
#ifdef __linux__
#include <libgen.h>
#endif

#if __APPLE__ &&  __aarch64__
#include "apple_arm_events.h"
#endif

struct event_count {
  std::chrono::duration<double> elapsed;
  std::vector<unsigned long long> event_counts;
  event_count() : elapsed(0), event_counts{0,0,0,0,0} {}
  event_count(const std::chrono::duration<double> _elapsed, const std::vector<unsigned long long> _event_counts) : elapsed(_elapsed), event_counts(_event_counts) {}
  event_count(const event_count& other): elapsed(other.elapsed), event_counts(other.event_counts) { }

  // The types of counters (so we can read the getter more easily)
  enum event_counter_types {
    CPU_CYCLES,
    INSTRUCTIONS,
    BRANCH_MISSES,
  };

  double elapsed_sec() const { return std::chrono::duration<double>(elapsed).count(); }
  double elapsed_ns() const { return std::chrono::duration<double, std::nano>(elapsed).count(); }
  double cycles() const { return static_cast<double>(event_counts[CPU_CYCLES]); }
  double instructions() const { return static_cast<double>(event_counts[INSTRUCTIONS]); }
  double branch_misses() const { return static_cast<double>(event_counts[BRANCH_MISSES]); }

  event_count& operator=(const event_count& other) {
    this->elapsed = other.elapsed;
    this->event_counts = other.event_counts;
    return *this;
  }
  event_count operator+(const event_count& other) const {
    return event_count(elapsed+other.elapsed, {
      event_counts[0]+other.event_counts[0],
      event_counts[1]+other.event_counts[1],
      event_counts[2]+other.event_counts[2],
      event_counts[3]+other.event_counts[3],
      event_counts[4]+other.event_counts[4],
    });
  }
  event_count operator/(size_t multiplier) const {
    return event_count(elapsed/multiplier, {
      event_counts[0]/multiplier,
      event_counts[1]/multiplier,
      event_counts[2]/multiplier,
      event_counts[3]/multiplier,
      event_counts[4]/multiplier,
    });
  }
  void operator+=(const event_count& other) {
    *this = *this + other;
  }
};

struct event_aggregate {
  bool has_events = false;
  int iterations = 0;
  event_count total{};
  event_count best{};
  event_count worst{};

  event_aggregate() = default;

  void operator<<(const event_count& other) {
    if (iterations == 0 || other.elapsed < best.elapsed) {
      best = other;
    }
    if (iterations == 0 || other.elapsed > worst.elapsed) {
      worst = other;
    }
    iterations++;
    total += other;
  }
  double total_elapsed_ns() const { return total.elapsed_ns(); }
  double elapsed_sec() const { return total.elapsed_sec() / iterations; }
  double elapsed_ns() const { return total.elapsed_ns() / iterations; }
  double cycles() const { return total.cycles() / iterations; }
  double instructions() const { return total.instructions() / iterations; }
};

struct event_collector {
  event_count count{};
  std::chrono::time_point<std::chrono::steady_clock> start_clock{};

#if defined(__linux__)
  LinuxEvents<PERF_TYPE_HARDWARE> linux_events;
  event_collector() : linux_events(std::vector<int>{
    PERF_COUNT_HW_CPU_CYCLES,
    PERF_COUNT_HW_INSTRUCTIONS,
  }) {}
  bool has_events() {
    return linux_events.is_working();
  }
#elif __APPLE__ &&  __aarch64__
  AppleEvents apple_events;
  performance_counters diff;
  event_collector() : diff(0) {
    apple_events.setup_performance_counters();
  }
  bool has_events() {
    return apple_events.setup_performance_counters();
  }
#else
  event_collector() {}
  bool has_events() {
    return false;
  }
#endif

  inline void start() {
#if defined(__linux)
    linux_events.start();
#elif __APPLE__ &&  __aarch64__
    if(has_events()) { diff = apple_events.get_counters(); }
#endif
    start_clock = std::chrono::steady_clock::now();
  }
  inline event_count& end() {
    const auto end_clock = std::chrono::steady_clock::now();
#if defined(__linux)
    linux_events.end(count.event_counts);
#elif __APPLE__ &&  __aarch64__
    if(has_events()) {
      performance_counters end = apple_events.get_counters();
      diff = end - diff;
    }
    count.event_counts[0] = diff.cycles;
    count.event_counts[1] = diff.instructions;
    count.event_counts[2] = diff.missed_branches;
    count.event_counts[3] = 0;
    count.event_counts[4] = 0;
#endif
    count.elapsed = end_clock - start_clock;
    return count;
  }
};

#endif