// This test proves that C++20 coroutine symmetric transfer runs in O(1) C
// stack: LLVM lowers `await_suspend` returning a handle into a musttail call
// to the next coroutine's resume function, and Fil-C preserves that as a
// frame-popping tail call. This works because the Fil-C ABI says that the
// callee roots its own incoming arguments, so the coroutine handle is rooted
// by the next coroutine's frame as soon as it starts running. This program
// recurses 1,000,000 deep.

#include <coroutine>
#include <cstdio>
#include <cstdlib>
#include <utility>

using namespace std;

template <typename T> struct Task {
    struct promise_type {
        T value{};
        coroutine_handle<> continuation = noop_coroutine();
        Task get_return_object()
        {
            return Task{coroutine_handle<promise_type>::from_promise(*this)};
        }
        suspend_always initial_suspend() noexcept { return {}; }
        struct FinalAwaiter {
            bool await_ready() noexcept { return false; }
            coroutine_handle<> await_suspend(coroutine_handle<promise_type> h) noexcept
            {
                return h.promise().continuation; // symmetric transfer
            }
            void await_resume() noexcept {}
        };
        FinalAwaiter final_suspend() noexcept { return {}; }
        void return_value(T v) { value = std::move(v); }
        void unhandled_exception() { abort(); }
    };

    coroutine_handle<promise_type> h;
    explicit Task(coroutine_handle<promise_type> hh) : h(hh) {}
    Task(Task&& other) : h(std::exchange(other.h, {})) {}
    Task(const Task&) = delete;
    ~Task()
    {
        if (h)
            h.destroy();
    }
    bool await_ready() { return false; }
    coroutine_handle<> await_suspend(coroutine_handle<> c)
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
    printf("deep=%ld\n", deep(depth).run());
    return 0;
}
