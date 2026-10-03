#include <coroutine>
#include <cstdio>
#include <exception>

struct Gen {
  struct promise_type {
    int value;
    Gen get_return_object() { return Gen{std::coroutine_handle<promise_type>::from_promise(*this)}; }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    std::suspend_always yield_value(int v) noexcept { value = v; return {}; }
    void return_void() {}
    void unhandled_exception() { std::terminate(); }
  };
  std::coroutine_handle<promise_type> h;
  ~Gen() { if (h) h.destroy(); }
  bool next() { h.resume(); return !h.done(); }
  int value() { return h.promise().value; }
};

Gen counter(int n) {
  int local[4] = {0};
  for (int i = 0; i < n; i++) { local[i & 3] += i; co_yield local[i & 3]; }
}

int main() {
  Gen g = counter(10);
  while (g.next()) printf("%d\n", g.value());
  return 0;
}
