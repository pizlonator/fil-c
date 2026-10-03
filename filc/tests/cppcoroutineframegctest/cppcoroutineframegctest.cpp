// GC stress for coroutine frames themselves: a generator with a large,
// over-aligned coroutine frame full of pointers to malloc'd buffers, GC
// cycles between every yield, verification of the frame contents after every
// resumption, and generators running on two threads.

#include <coroutine>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <thread>
#include <utility>

#include <stdfil.h>
#include <filc_test_support.h>

using namespace std;

static constexpr unsigned num_buffers = 256;
static constexpr unsigned base_rounds = 3;

struct Gen {
    struct alignas(64) promise_type {
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

static void make_name(unsigned id, unsigned i, char out[64])
{
    snprintf(out, 64, "gen-%u-%u", id, i);
}

static Gen gen(unsigned id)
{
    // A big array of pointers to malloc'd buffers, living in the coroutine
    // frame. The GC must scan the whole frame while we are suspended.
    char* ptrs[num_buffers];
    char name[64];
    for (unsigned i = num_buffers; i--;) {
        make_name(id, i, name);
        ptrs[i] = strdup(name);
    }
    for (unsigned i = 0; i < num_buffers; ++i) {
        make_name(id, i, name);
        co_yield ptrs[i];
        // After resumption, the whole frame must be intact.
        for (unsigned j = 0; j < num_buffers; ++j) {
            make_name(id, j, name);
            if (strcmp(ptrs[j], name)) {
                printf("gen %u: buffer %u corrupted\n", id, j);
                abort();
            }
        }
    }
    for (unsigned i = num_buffers; i--;)
        free(ptrs[i]);
}

static void gen_thread(unsigned id, unsigned rounds)
{
    for (unsigned round = 0; round < rounds; ++round) {
        Gen g = gen(id * 100 + round);
        char name[64];
        for (unsigned i = 0; i < num_buffers; ++i) {
            ZASSERT(g.next());
            make_name(id * 100 + round, i, name);
            // GC between every yield. The yielded buffer is referenced only by
            // the coroutine frame and by us.
            if (id == 0)
                zgc_request_and_wait();
            if (strcmp(g.value(), name)) {
                printf("gen %u: yielded buffer %u corrupted: %s\n", id, i, g.value());
                abort();
            }
        }
        ZASSERT(!g.next());
    }
}

int main()
{
    unsigned rounds = base_rounds;
    if (zgc_is_scribbling())
        rounds = 1;
    if (zgc_is_verifying())
        rounds = 1;
    thread other(gen_thread, 1, rounds);
    gen_thread(0, rounds);
    other.join();
    zprintf("Sukces!\n");
    return 0;
}
