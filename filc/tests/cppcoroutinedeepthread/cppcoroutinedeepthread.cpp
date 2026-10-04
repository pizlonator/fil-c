// Thread variant of cppcoroutinedeep: this proves that C++20 coroutine
// symmetric transfer runs in O(1) C stack even on a thread with Fil-C's
// default 80 KB thread stack. LLVM lowers `await_suspend` returning a handle
// into a musttail call to the next coroutine's resume function, and Fil-C
// preserves that as a frame-popping tail call, since the Fil-C ABI says that
// the callee roots its own incoming arguments. This program recurses
// 1,000,000 deep on a pthread.

#include <coroutine>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <utility>

template <typename T> struct Task {
    struct promise_type {
        T value{};
        std::coroutine_handle<> continuation = std::noop_coroutine();
        Task get_return_object()
        {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        struct FinalAwaiter {
            bool await_ready() noexcept { return false; }
            std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept
            {
                return h.promise().continuation; // symmetric transfer
            }
            void await_resume() noexcept {}
        };
        FinalAwaiter final_suspend() noexcept { return {}; }
        void return_value(T v) { value = std::move(v); }
        void unhandled_exception() { abort(); }
    };

    std::coroutine_handle<promise_type> h;
    explicit Task(std::coroutine_handle<promise_type> hh) : h(hh) {}
    Task(Task&& other) : h(std::exchange(other.h, {})) {}
    Task(const Task&) = delete;
    ~Task()
    {
        if (h)
            h.destroy();
    }
    bool await_ready() { return false; }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> c)
    {
        h.promise().continuation = c;
        return h; // symmetric transfer
    }
    T await_resume() { return std::move(h.promise().value); }
    T run()
    {
        h.resume();
        return await_resume();
    }
};

static Task<long> deep(int depth)
{
    if (!depth)
        co_return 0;
    long r = co_await deep(depth - 1);
    co_return r + 1;
}

int main(int argc, char** argv)
{
    int depth = 1000000;
    if (argc > 1)
        depth = atoi(argv[1]);
    long result = 0;
    std::thread t([&] { result = deep(depth).run(); });
    t.join();
    printf("deep=%ld\n", result);
    return 0;
}
