// GC stress for C++20 coroutine symmetric transfer: deep chains of
// symmetric-transfer awaits run while another thread triggers GC cycles; a
// tower of suspended coroutines whose frames hold the only strong references
// to their children and to malloc'd buffers across GC cycles; and weak
// references to coroutine frames. This exercises the rooting of coroutine
// handles passed across symmetric-transfer calls and the scanning of
// suspended coroutine frames by the GC.

#include <coroutine>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <atomic>
#include <thread>
#include <utility>

#include <stdfil.h>
#include <filc_test_support.h>

using namespace std;

static atomic<bool> done_flag;
static atomic<long> gc_cycles;

static void gc_hammer()
{
    while (!done_flag.load(memory_order_acquire)) {
        zgc_request_and_wait();
        gc_cycles.fetch_add(1, memory_order_relaxed);
    }
}

// ---------- symmetric transfer chains ----------

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

static Task<long> link(int depth, long salt)
{
    if (!depth) {
        // Allocate in the leaf so that the chain allocates while the GC races
        // with the symmetric transfers.
        char* buf = (char*)malloc(64);
        snprintf(buf, 64, "link-%ld", salt);
        char want[64];
        snprintf(want, 64, "link-%ld", salt);
        if (strcmp(buf, want)) {
            printf("link corruption: %s vs %s\n", buf, want);
            abort();
        }
        free(buf);
        co_return salt;
    }
    long r = co_await link(depth - 1, salt + 1);
    co_return r + 1;
}

// NOTE: this runs a depth-10000 symmetric-transfer chain, which consumes
// about 3.5 MB of the 8 MB main-thread stack in Fil-C (two C frames per
// level; see the FIXME in cppcoroutinedeep). This test needs a default-sized
// main stack and will fail under a smaller `ulimit -s` (so does
// cppcoroutinetask, which does the same thing at the same depth).

static void chain_test(int depth, long runs)
{
    for (long i = runs; i--;) {
        long salt = 1000000 + i;
        ZASSERT(link(depth, salt).run() == salt + 2 * depth);
    }
}

// ---------- tower of suspended coroutines ----------

struct Node {
    struct promise_type {
        long result = 0;
        Node get_return_object()
        {
            return Node{coroutine_handle<promise_type>::from_promise(*this)};
        }
        suspend_always initial_suspend() noexcept { return {}; }
        suspend_always final_suspend() noexcept { return {}; }
        void return_value(long v) { result = v; }
        void unhandled_exception() { abort(); }
    };

    coroutine_handle<promise_type> h;
    Node() {}
    Node(coroutine_handle<promise_type> hh) : h(hh) {}
    Node(Node&& other) : h(std::exchange(other.h, {})) {}
    Node(const Node&) = delete;
    ~Node()
    {
        if (h)
            h.destroy();
    }
};

// Builds a tower of suspended coroutines. Each level holds the only strong
// reference to its child in its own coroutine frame; the leaf holds the only
// strong reference to a malloc'd buffer. The driver runs GC cycles while the
// whole tower is suspended.
static Node tower(int depth, long magic)
{
    if (!depth) {
        char* buf = (char*)malloc(96);
        snprintf(buf, 96, "tower-%ld", magic);
        // GC while this buffer is only rooted by our coroutine frame.
        zgc_request_and_wait();
        co_await suspend_always{};
        char want[96];
        snprintf(want, 96, "tower-%ld", magic);
        if (strcmp(buf, want)) {
            printf("tower leaf corruption: %s vs %s\n", buf, want);
            abort();
        }
        free(buf);
        co_return magic;
    }
    Node child = tower(depth - 1, magic + 1);
    // GC while the child is suspended and only rooted by our frame.
    zgc_request_and_wait();
    co_await suspend_always{};
    while (!child.h.done())
        child.h.resume();
    co_return child.h.promise().result + 1;
}

static void tower_test(int depth, long magic)
{
    Node top = tower(depth, magic);
    for (int round = 0; round < depth + 2 && !top.h.done(); ++round) {
        top.h.resume();
        zgc_request_and_wait();
    }
    ZASSERT(top.h.done());
    ZASSERT(top.h.promise().result == magic + 2 * depth);
}

// ---------- weak references ----------

static Node weak_coro(long magic)
{
    char* buf = (char*)malloc(96);
    snprintf(buf, 96, "weak-%ld", magic);
    co_await suspend_always{};
    char want[96];
    snprintf(want, 96, "weak-%ld", magic);
    ZASSERT(!strcmp(buf, want));
    free(buf);
    co_return magic;
}

static void weak_test()
{
    // While a coroutine frame is strongly referenced, a weak reference to it
    // stays alive across GC cycles.
    Node victim = weak_coro(7);
    zweak* weak = zweak_new(victim.h.address());
    zgc_request_and_wait();
    zgc_request_and_wait();
    ZASSERT(zweak_get(weak) == victim.h.address());
    victim.h.resume();
    victim.h.resume();
    ZASSERT(victim.h.done());
    victim.h.destroy();
    victim.h = {};
    zgc_request_and_wait();
    zgc_request_and_wait();
    ZASSERT(zweak_get(weak) == NULL);

    // A suspended coroutine frame with no strong references gets collected by
    // the GC without anyone calling destroy().
    Node ghost = weak_coro(13);
    zweak* ghost_weak = zweak_new(ghost.h.address());
    ghost.h = {}; // drop the only strong reference
    zgc_request_and_wait();
    zgc_request_and_wait();
    ZASSERT(zweak_get(ghost_weak) == NULL);
}

int main()
{
    thread hammer(gc_hammer);
    chain_test(10000, 8);
    tower_test(8, 1000);
    weak_test();
    done_flag.store(true, memory_order_release);
    hammer.join();
    zprintf("Sukces! gc_cycles = %ld\n", gc_cycles.load());
    return 0;
}
