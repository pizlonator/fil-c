// C++20 coroutine GC test at -O0. At zero optimization the coroutine frame has
// a different layout (every alloca gets its own frame slot), and this test
// also covers the coroutine lowering pipeline for unoptimized code.

#include <coroutine>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <utility>
#include <vector>

#include <stdfil.h>
#include <filc_test_support.h>

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

static Task<long> link(int depth)
{
    if (!depth)
        co_return 0;
    long r = co_await link(depth - 1);
    co_return r + 1;
}

struct Gen {
    struct promise_type {
        char* current = nullptr;
        Gen get_return_object()
        {
            return Gen{coroutine_handle<promise_type>::from_promise(*this)};
        }
        suspend_always initial_suspend() noexcept { return {}; }
        suspend_always final_suspend() noexcept { return {}; }
        suspend_always yield_value(char* value) noexcept
        {
            current = value;
            return {};
        }
        void return_void() {}
        void unhandled_exception() { abort(); }
    };

    coroutine_handle<promise_type> h;
    explicit Gen(coroutine_handle<promise_type> hh) : h(hh) {}
    Gen(Gen&& other) : h(std::exchange(other.h, {})) {}
    Gen(const Gen&) = delete;
    ~Gen()
    {
        if (h)
            h.destroy();
    }
    bool next() { h.resume(); return !h.done(); }
    char* value() { return h.promise().current; }
};

static Gen gen(int id)
{
    // Several distinct locals to exercise the coroutine frame layout.
    char* a = (char*)malloc(64);
    snprintf(a, 64, "gen-%d", id);
    vector<int> b(8, id);
    long c = 0;
    for (int i = 0; i < 8; ++i) {
        co_yield a;
        b[i & 7] += i;
        c += b[i & 7];
        // The frame must be intact after every resumption.
        char want[64];
        snprintf(want, 64, "gen-%d", id);
        ZASSERT(!strcmp(a, want));
        ZASSERT(c == (long)id * (i + 1) + (long)i * (i + 1) / 2);
    }
    free(a);
}

struct Susp {
    struct promise_type {
        long result = 0;
        Susp get_return_object()
        {
            return Susp{coroutine_handle<promise_type>::from_promise(*this)};
        }
        suspend_always initial_suspend() noexcept { return {}; }
        suspend_always final_suspend() noexcept { return {}; }
        void return_value(long v) { result = v; }
        void unhandled_exception() { abort(); }
    };

    coroutine_handle<promise_type> h;
    explicit Susp(coroutine_handle<promise_type> hh) : h(hh) {}
    Susp(Susp&& other) : h(std::exchange(other.h, {})) {}
    Susp(const Susp&) = delete;
    ~Susp()
    {
        if (h)
            h.destroy();
    }
};

static Susp susp(long magic)
{
    char* buf = (char*)malloc(96);
    snprintf(buf, 96, "susp-%ld", magic);
    co_await suspend_always{};
    char want[96];
    snprintf(want, 96, "susp-%ld", magic);
    ZASSERT(!strcmp(buf, want));
    free(buf);
    co_return magic;
}

int main()
{
    // Symmetric transfer chain.
    ZASSERT(link(1000).run() == 1000);

    // Generator with GC between yields.
    Gen g = gen(1);
    for (int i = 0; i < 8; ++i) {
        ZASSERT(g.next());
        char want[64];
        snprintf(want, 64, "gen-%d", 1);
        zgc_request_and_wait();
        ZASSERT(!strcmp(g.value(), want));
    }
    ZASSERT(!g.next());

    // A suspended coroutine frame surviving GC cycles.
    Susp s = susp(42);
    s.h.resume(); // runs to the middle suspension
    zgc_request_and_wait();
    zgc_request_and_wait();
    s.h.resume(); // verifies its buffer and finishes
    ZASSERT(s.h.done());
    ZASSERT(s.h.promise().result == 42);

    zprintf("Sukces!\n");
    return 0;
}
