// Many suspended coroutines whose frames hold the only references to heap
// data, a custom promise operator new/delete, from_address round trips,
// resumption from other threads, and GC cycles in between.
#include <coroutine>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <stdfil.h>

static long allocs, frees;

struct Co {
  struct promise_type {
    std::string *out = nullptr;
    static void *operator new(size_t n) { allocs++; return ::operator new(n); }
    static void operator delete(void *p) { frees++; ::operator delete(p); }
    Co get_return_object() { return {std::coroutine_handle<promise_type>::from_promise(*this)}; }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() { abort(); }
  };
  std::coroutine_handle<promise_type> h;
};

Co worker(int id, std::string *out) {
  auto owned = std::make_unique<std::string>("worker-" + std::to_string(id));
  std::vector<int> v(id % 50 + 1, id);
  co_await std::suspend_always{};
  char *heap = (char *)malloc(64);
  snprintf(heap, 64, "%s:%zu", owned->c_str(), v.size());
  co_await std::suspend_always{};
  *out = heap;
  free(heap);
}

int main() {
  const int N = 2000;
  std::vector<std::string> outs(N);
  std::vector<void *> addrs;
  for (int i = 0; i < N; i++) addrs.push_back(worker(i, &outs[i]).h.address());
  zgc_request_and_wait();
  for (void *a : addrs) std::coroutine_handle<>::from_address(a).resume();
  zgc_request_and_wait();
  std::vector<std::thread> ts;
  for (int t = 0; t < 4; t++)
    ts.emplace_back([&, t] {
      for (int i = t; i < N; i += 4) {
        auto h = std::coroutine_handle<>::from_address(addrs[i]);
        h.resume();
        h.resume();
      }
    });
  for (auto &t : ts) t.join();
  zgc_request_and_wait();
  for (int i = 0; i < N; i++) {
    auto h = std::coroutine_handle<Co::promise_type>::from_address(addrs[i]);
    if (!h.done()) { puts("not done"); return 1; }
    h.destroy();
    std::string want = "worker-" + std::to_string(i) + ":" + std::to_string(i % 50 + 1);
    if (outs[i] != want) { printf("mismatch %d: %s\n", i, outs[i].c_str()); return 1; }
  }
  printf("ok allocs=%ld frees=%ld\n", allocs, frees);
}
