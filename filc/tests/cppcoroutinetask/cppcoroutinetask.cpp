// Lazy tasks with symmetric transfer, exceptions, noop_coroutine, strings.
#include <coroutine>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

template <typename T> struct Task {
  struct promise_type {
    T value{};
    std::exception_ptr exc;
    std::coroutine_handle<> continuation = std::noop_coroutine();
    Task get_return_object() { return Task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
    std::suspend_always initial_suspend() noexcept { return {}; }
    struct FinalAwaiter {
      bool await_ready() noexcept { return false; }
      std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
        return h.promise().continuation;  // symmetric transfer
      }
      void await_resume() noexcept {}
    };
    FinalAwaiter final_suspend() noexcept { return {}; }
    void return_value(T v) { value = std::move(v); }
    void unhandled_exception() { exc = std::current_exception(); }
  };
  std::coroutine_handle<promise_type> h;
  explicit Task(std::coroutine_handle<promise_type> h) : h(h) {}
  Task(Task &&o) : h(std::exchange(o.h, {})) {}
  ~Task() { if (h) h.destroy(); }
  bool await_ready() { return false; }
  std::coroutine_handle<> await_suspend(std::coroutine_handle<> c) {
    h.promise().continuation = c;
    return h;
  }
  T await_resume() {
    if (h.promise().exc) std::rethrow_exception(h.promise().exc);
    return std::move(h.promise().value);
  }
  T run() {
    h.resume();
    return await_resume();
  }
};

Task<std::string> leaf(int i) {
  if (i == 7) throw std::runtime_error("seven");
  co_return "leaf" + std::to_string(i);
}

Task<std::string> middle(int n) {
  std::string acc;
  for (int i = 0; i < n; i++) {
    try {
      acc += co_await leaf(i);
    } catch (const std::exception &e) {
      acc += std::string("[") + e.what() + "]";
    }
    acc += ",";
  }
  co_return acc;
}

Task<long> deep(int d) {
  if (d == 0) co_return 1;
  long r = co_await deep(d - 1);
  co_return r + 1;
}

#include <cstdlib>
int main(int argc, char **argv) {
  int depth = argc > 1 ? atoi(argv[1]) : 10000;
  printf("%s\n", middle(9).run().c_str());
  printf("deep=%ld\n", deep(depth).run());  // symmetric transfer uses stack under Fil-C
  return 0;
}
