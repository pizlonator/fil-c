// Resuming a destroyed coroutine must be caught by Fil-C.
#include <coroutine>
#include <cstdio>
struct R {
  struct promise_type {
    R get_return_object() { return {std::coroutine_handle<promise_type>::from_promise(*this)}; }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() {}
  };
  std::coroutine_handle<promise_type> h;
};
R f() { puts("body"); co_await std::suspend_always{}; puts("after"); }
int main() {
  R r = f();
  r.h.resume();
  r.h.destroy();
  puts("resuming destroyed coroutine...");
  r.h.resume();
  puts("SHOULD NOT GET HERE");
}
