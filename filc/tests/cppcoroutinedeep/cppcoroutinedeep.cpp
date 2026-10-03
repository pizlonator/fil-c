// FIXME(bug): C++20 coroutine symmetric transfer is supposed to consume O(1)
// C stack: LLVM lowers `await_suspend` returning a handle into a musttail
// call to the next coroutine's resume function. Fil-C demotes that musttail
// call into an ordinary call, because the Fil-C ABI says that the caller
// roots the arguments it passes, and the coroutine handle returned by
// await_suspend is only rooted by the frame of the coroutine doing the
// transfer: popping that frame would leave the next coroutine's frame
// unrooted, so the GC could free it while it runs. The demoted call means
// that a chain of N symmetric transfers uses O(N) C stack, so deep
// symmetric-transfer chains overflow the stack. The stack is spent twice per
// chain level: the descent into the chain nests a .resume frame per level
// (about 353 bytes per level on the main thread), and the ascent nests
// frames again as each parent's destructor recursively destroys its child
// coroutine from inside the parent's .resume frame. On the 8 MiB main stack
// this program overflows at depth 23735, while ordinary C++ compilers (which
// keep the musttail) run it at depth 10,000,000. Threads are the practically
// important case, since coroutine runtimes run tasks on worker threads: Fil-C
// pthreads default to an 80 KB stack (pthread_getattr_np reports
// size=81920), and the same chain on a thread overflows at a depth of only
// about 140 (about 600 bytes per level; see cppcoroutinedeepthread). Fil-C
// catches the overflow itself rather than segfaulting ("filc safety error:
// stack overflow.", followed by a backtrace and the generic "filc panic:
// thwarted a futile attempt to violate memory safety." line), so this is a
// compatibility limitation, not a memory safety bug. If this is ever fixed
// (for example by giving the callee a GC-safe way to root the handle before
// the caller's frame is popped), remove `skip` from the manifest and change
// `return` to success.
//
// Until then, this test is skipped. Run it with `filc/run-tests -t
// cppcoroutinedeep` to see the failure.

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
