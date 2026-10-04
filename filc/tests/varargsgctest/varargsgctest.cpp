#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <atomic>
#include <thread>

#include <stdfil.h>
#include <filc_test_support.h>
using namespace std;

static constexpr unsigned num_pointers = 40; // CC pack > 256 bytes -> outline buffer
static constexpr long magic_base = 1000000000;
static constexpr unsigned num_threads = 4;
static constexpr unsigned base_iterations = 30000;
static constexpr unsigned base_zcall_iterations = 2000;

static atomic<bool> done_flag;
static atomic<long> gc_cycles;
static atomic<unsigned> threads_done;

struct thing {
    long magic;
    long payload;
};

static thing* make_thing(unsigned seed)
{
    thing* t = (thing*)malloc(sizeof(thing));
    t->magic = magic_base + seed;
    t->payload = (long)seed * 3;
    return t;
}

static long check_varargs(int count, ...)
{
    va_list list;
    va_start(list, count);
    long total = 0;
    for (int i = 0; i < count; ++i) {
        thing* t = va_arg(list, thing*);
        ZASSERT(t->magic == magic_base + (long)(unsigned)(t->payload / 3));
        total += t->payload;
    }
    va_end(list);
    return total;
}

static long expected_total(unsigned seed)
{
    long total = 0;
    for (unsigned i = 0; i < num_pointers; ++i)
        total += (long)(seed + i) * 3;
    return total;
}

static long call_with_many(unsigned seed)
{
    thing* things[num_pointers];
    for (unsigned i = num_pointers; i--;)
        things[i] = make_thing(seed + i);
    long result = check_varargs(
        (int)num_pointers,
        things[0], things[1], things[2], things[3], things[4], things[5], things[6], things[7],
        things[8], things[9], things[10], things[11], things[12], things[13], things[14],
        things[15], things[16], things[17], things[18], things[19], things[20], things[21],
        things[22], things[23], things[24], things[25], things[26], things[27], things[28],
        things[29], things[30], things[31], things[32], things[33], things[34], things[35],
        things[36], things[37], things[38], things[39]);
    for (unsigned i = num_pointers; i--;)
        ZASSERT(things[i]->magic == magic_base + seed + i);
    return result;
}

static void thread_main(unsigned tid)
{
    unsigned iterations = base_iterations;
    if (zgc_is_scribbling())
        iterations /= 4;
    if (zgc_is_verifying())
        iterations /= 4;
    for (unsigned i = iterations; i--;) {
        long result = call_with_many(tid * 1000000 + i);
        ZASSERT(result == expected_total(tid * 1000000 + i));
    }
    threads_done.fetch_add(1, memory_order_release);
}

struct varargs_pack {
    int count;
    thing* things[num_pointers];
};

static void zcall_varargs_main(unsigned tid)
{
    unsigned iterations = base_zcall_iterations;
    if (zgc_is_scribbling())
        iterations /= 4;
    if (zgc_is_verifying())
        iterations /= 4;
    for (unsigned i = iterations; i--;) {
        varargs_pack* pack = (varargs_pack*)malloc(sizeof(varargs_pack));
        pack->count = (int)num_pointers;
        unsigned seed = tid * 1000000 + i + 7;
        for (unsigned j = num_pointers; j--;)
            pack->things[j] = make_thing(seed + j);
        long result = *(long*)zcall((void*)check_varargs, pack);
        ZASSERT(result == expected_total(seed));
    }
    threads_done.fetch_add(1, memory_order_release);
}

int main()
{
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
    vector<thread> zcall_threads;
    for (unsigned i = num_threads; i--;)
        zcall_threads.push_back(thread(zcall_varargs_main, i));

    while (threads_done.load(memory_order_acquire) < num_threads * 2) {
        usleep(100000);
        zprintf("threads_done = %u, gc_cycles = %ld\n",
                threads_done.load(memory_order_acquire), gc_cycles.load());
    }

    for (thread& t : threads)
        t.join();
    for (thread& t : zcall_threads)
        t.join();

    done_flag.store(true, memory_order_release);
    hammer.join();

    zprintf("Sukces! gc_cycles = %ld\n", gc_cycles.load());
    return 0;
}
