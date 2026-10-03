// Multi-threaded GC stress test using C++20 coroutines: many suspended
// coroutines whose frames hold the only strong references to hash maps, GC
// storms from a separate thread while the coroutines are being resumed from
// other threads, short-lived coroutine frame churn, and per-batch integrity
// checks of coroutine frame contents. Modeled on the other *gctest tests.

#include <coroutine>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#include <atomic>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <stdfil.h>
#include <filc_test_support.h>

using namespace std;

static atomic<long> promise_allocs;
static atomic<long> promise_frees;
static atomic<long> gc_cycles;
static atomic<bool> done_flag;
static atomic<unsigned> threads_done;

static inline uint32_t xorshift32(uint32_t& state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

struct Worker {
    struct promise_type {
        unordered_map<int, long> map;
        long checksum = 0;
        unsigned ops_done = 0;
        bool done = false;

        Worker get_return_object()
        {
            return Worker{coroutine_handle<promise_type>::from_promise(*this)};
        }
        suspend_always initial_suspend() noexcept { return {}; }
        suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { abort(); }
        static void* operator new(size_t size)
        {
            promise_allocs.fetch_add(1, memory_order_relaxed);
            return ::operator new(size);
        }
        static void operator delete(void* ptr)
        {
            promise_frees.fetch_add(1, memory_order_relaxed);
            ::operator delete(ptr);
        }
    };

    coroutine_handle<promise_type> h;
    Worker() {}
    Worker(coroutine_handle<promise_type> hh) : h(hh) {}
    Worker(Worker&& other) : h(std::exchange(other.h, {})) {}
    Worker(const Worker&) = delete;
    ~Worker()
    {
        if (h)
            h.destroy();
    }
};

// Awaiter that gives the coroutine body its own handle.
struct Self {
    coroutine_handle<Worker::promise_type> h{};
    bool await_ready() { return false; }
    void await_suspend(coroutine_handle<Worker::promise_type> hh) { h = hh; }
    coroutine_handle<Worker::promise_type> await_resume() { return h; }
};

Worker worker(unsigned seed, unsigned batches)
{
    auto self = co_await Self{};
    auto& p = self.promise();
    uint32_t rng = seed * 2654435761u + 1;
    for (unsigned batch = 0; batch < batches; ++batch) {
        unsigned ops = 8 + (xorshift32(rng) & 31);
        for (unsigned i = ops; i--;) {
            int key = (int)(xorshift32(rng) & 1023);
            long value = (long)xorshift32(rng);
            auto iter = p.map.find(key);
            if (iter != p.map.end())
                p.checksum -= iter->second;
            p.map[key] = value;
            p.checksum += value;
            if ((xorshift32(rng) & 7) == 0 && p.map.size() > 16) {
                int dead_key = (int)(xorshift32(rng) & 1023);
                auto dead_iter = p.map.find(dead_key);
                if (dead_iter != p.map.end()) {
                    p.checksum -= dead_iter->second;
                    p.map.erase(dead_iter);
                }
            }
        }
        p.ops_done++;
        co_await suspend_always{};
    }
    long sum = 0;
    for (auto& entry : p.map)
        sum += entry.second;
    if (sum != p.checksum) {
        printf("worker corruption: seed %u: sum %ld != checksum %ld\n", seed, sum, p.checksum);
        abort();
    }
    p.done = true;
}
static void verify(Worker& w)
{
    auto& p = w.h.promise();
    long sum = 0;
    for (auto& entry : p.map)
        sum += entry.second;
    ZASSERT(sum == p.checksum);
}

// Short-lived coroutine that runs to completion as soon as it is created
// (suspend_never initial and final suspend). This allocates and frees a
// coroutine frame for every call, while the GC storms. The promise's
// get_return_object returns an empty object, so no dangling handle ever
// escapes.
struct Churn {
    struct promise_type {
        Churn get_return_object() { return Churn{}; }
        suspend_never initial_suspend() noexcept { return {}; }
        suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { abort(); }
    };
};

static Churn churn(unsigned seed)
{
    vector<int> v(48, (int)seed);
    long sum = 0;
    for (int x : v)
        sum += x;
    if (sum != 48L * (long)(int)seed)
        abort();
    co_return;
}

// Short-lived coroutine that suspends in the middle and must be destroyed
// explicitly.
struct Churn2 {
    struct promise_type {
        Churn2 get_return_object()
        {
            return Churn2{coroutine_handle<promise_type>::from_promise(*this)};
        }
        suspend_always initial_suspend() noexcept { return {}; }
        suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { abort(); }
    };

    coroutine_handle<promise_type> h;
    Churn2() {}
    Churn2(coroutine_handle<promise_type> hh) : h(hh) {}
    Churn2(Churn2&& other) : h(std::exchange(other.h, {})) {}
    Churn2(const Churn2&) = delete;
    ~Churn2()
    {
        if (h)
            h.destroy();
    }
};

Churn2 churn2(unsigned seed)
{
    vector<long> v(64);
    for (unsigned i = 64; i--;)
        v[i] = (long)(seed + i);
    co_await suspend_always{};
    long sum = 0;
    for (long x : v)
        sum += x;
    ZASSERT(sum == 64L * (long)seed + 2016L); // 2016 = 0 + 1 + ... + 63
}

static constexpr unsigned num_threads = 8;
static constexpr unsigned coros_per_thread = 128;
static constexpr unsigned base_batches = 24;
static constexpr unsigned base_churn = 3000;
static constexpr unsigned base_churn2 = 500;

static void thread_main(unsigned tid)
{
    uint32_t rng = tid * 977 + 12345;
    unsigned batches = base_batches;
    unsigned churn_count = base_churn;
    unsigned churn2_count = base_churn2;
    if (zgc_is_scribbling()) {
        batches /= 2;
        churn_count /= 2;
        churn2_count /= 2;
    }
    if (zgc_is_verifying()) {
        batches /= 2;
        churn_count /= 2;
        churn2_count /= 2;
    }

    vector<Worker> workers;
    workers.reserve(coros_per_thread);
    for (unsigned i = coros_per_thread; i--;)
        workers.push_back(worker(tid * 100003 + i, batches));

    for (unsigned batch = 0; batch < batches + 1; ++batch) {
        for (Worker& w : workers) {
            w.h.resume();
            if (!w.h.done() && !(batch & 3))
                verify(w);
        }
        // Churn coroutine frames while the GC storms.
        for (unsigned i = churn_count / batches; i--;)
            churn(xorshift32(rng));
    }

    for (Worker& w : workers) {
        w.h.resume();
        ZASSERT(w.h.done());
        ZASSERT(w.h.promise().ops_done == batches);
        verify(w);
    }

    for (unsigned i = churn2_count; i--;) {
        Churn2 c = churn2(xorshift32(rng));
        c.h.resume();
        c.h.resume();
        ZASSERT(c.h.done());
    }

    threads_done.fetch_add(1, memory_order_release);
}

int main()
{
    zprintf("cppcoroutinegctest starting\n");

    thread hammer([] {
        while (!done_flag.load(memory_order_acquire)) {
            if (gc_cycles.load(memory_order_relaxed) < 4000) {
                zgc_request_and_wait();
                gc_cycles.fetch_add(1, memory_order_relaxed);
            } else
                usleep(1000);
        }
    });

    vector<thread> threads;
    for (unsigned i = num_threads; i--;)
        threads.push_back(thread(thread_main, i));

    while (threads_done.load(memory_order_acquire) < num_threads) {
        usleep(100000);
        zprintf("threads_done = %u, gc_cycles = %ld\n",
                threads_done.load(memory_order_acquire), gc_cycles.load());
    }

    for (thread& t : threads)
        t.join();

    done_flag.store(true, memory_order_release);
    hammer.join();

    ZASSERT(promise_allocs.load() == promise_frees.load());
    zprintf("Sukces! gc_cycles = %ld allocs = %ld frees = %ld\n", gc_cycles.load(),
            promise_allocs.load(), promise_frees.load());
    return 0;
}
