#include <atomic>
#include <thread>
#include <vector>
#include <cstdio>
#include <cstdint>
// Is it the failed-CAS path (expected written back) or the loaded value?
static std::atomic<uint64_t *> slots[4096];
static std::atomic<unsigned> counter{0}, lost_load{0}, lost_cas{0};
#include <stdfil.h>
int main() {
  std::vector<std::thread> ts;
  for (int t = 0; t < 8; t++) ts.emplace_back([] {
    for (int i = 0; i < 20000; i++) {
      unsigned idx = counter.fetch_add(1, std::memory_order_relaxed);
      auto & s = slots[(idx / 17) % 4096];
      uint64_t * p = s.load(std::memory_order_acquire);
      if (p) { if (!zhasvalidcap(p)) lost_load++; continue; }
      uint64_t * n = new uint64_t[17];
      uint64_t * expected = nullptr;
      if (!s.compare_exchange_strong(expected, n, std::memory_order_release, std::memory_order_acquire)) {
        if (!zhasvalidcap(expected)) lost_cas++;
        delete[] n;
      }
    }
  });
  for (auto & t : ts) t.join();
  std::printf("lost after load: %u, lost after failed CAS: %u\n", lost_load.load(), lost_cas.load());
}
