// FIXME(bug): Thread variant of cppcoroutinedeep: runs a deep C++20
// coroutine symmetric-transfer chain on a pthread. Fil-C demotes the
// musttail call that LLVM emits for symmetric transfer into an ordinary
// call (the Fil-C ABI says that the caller roots the arguments it passes,
// and the handle returned by await_suspend is only rooted by the frame of
// the coroutine doing the transfer), so the chain consumes O(depth) C
// stack, and the stack is spent twice per level: the descent nests a
// .resume frame per level, and the ascent nests frames again as each
// parent's destructor recursively destroys its child coroutine from inside
// the parent's .resume frame. Fil-C pthreads default to an 80 KB stack
// (pthread_getattr_np reports size=81920), so this program overflows at a
// depth of only about 140 on a thread (about 600 bytes per level), while
// ordinary C++ compilers (which keep the musttail) run it at depth
// 10,000,000. Fil-C catches the overflow itself rather than segfaulting
// ("filc safety error: stack overflow.", followed by a backtrace through
// deep(int) (.resume) and the generic "filc panic: thwarted a futile
// attempt to violate memory safety." line), so this is a compatibility
// limitation, not a memory safety bug. Coroutine runtimes usually run tasks
// on worker threads, so this is the practically important case. If this is
// ever fixed (for example by giving the callee a GC-safe way to root the
// handle before the caller's frame is popped), remove `skip` from the
// manifest and change `return` to success.
//
// Until then, this test is skipped. Run it with `filc/run-tests -t
// cppcoroutinedeepthread` to see the failure.

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
