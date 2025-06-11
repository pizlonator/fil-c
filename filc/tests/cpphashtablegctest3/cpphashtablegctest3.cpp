#define _GNU_SOURCE

#include <iostream>
#include <unordered_map>
#include <string>
#include <stdfil.h>
#include <sstream>
#include <inttypes.h>
#include <thread>
#include <mutex>
#include <unistd.h>
#include <filc_test_support.h>

using namespace std;

namespace {

constexpr size_t num_threads = 7;
constexpr size_t base_repeats = 70000;
constexpr size_t num_toplevel_keys = 10000;
constexpr size_t num_inner_keys = 10000;
constexpr size_t max_nodes_to_add = 20;
constexpr size_t max_nodes_to_remove = 20;

constexpr unsigned selection_range = 100;
constexpr unsigned add_nodes_probability = 55;
constexpr unsigned remove_top_level_probability = 9;
constexpr unsigned remove_inner_probability = 26;
constexpr unsigned move_to_global_probability = 5;
constexpr unsigned move_from_global_probability = 5;

unsigned fast_random_state = 42;

inline uint32_t xorshift32(uint32_t state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

inline unsigned get_fast_random(unsigned upper_bound)
{
    unsigned rand_value;

    if (!upper_bound)
        upper_bound = UINT32_MAX;

    fast_random_state = xorshift32(fast_random_state);
    rand_value = fast_random_state % upper_bound;

    return rand_value;
}

zgc_finq* finalizer_queue = zgc_finq_new();

struct inner_map {
    unordered_map<string, string> map;

    void* operator new(size_t size)
    {
        return zgc_finq_alloc(finalizer_queue, size);
    }
};

mutex global_lock;
unordered_map<string, inner_map*> global_hash_map;
unsigned num_threads_done;

struct LocalTester {
    void addNodes()
    {
        string key;
        {
            ostringstream buf;
            buf << "key " << get_fast_random(num_toplevel_keys);
            key = buf.str();
        }
        inner_map* inner;
        auto result = hash_map.emplace(key, nullptr);
        if (result.second)
            result.first->second = new inner_map;
        inner = result.first->second;
        for (size_t count = get_fast_random(max_nodes_to_add); count--;) {
            string subkey;
            string value;
            {
                ostringstream buf;
                buf << "subkey " << get_fast_random(num_inner_keys);
                subkey = buf.str();
            }
            {
                ostringstream buf;
                buf << "subvalue " << get_fast_random(0);
                value = buf.str();
            }
            inner->map[subkey] = value;
        }
    }

    void removeTopLevel()
    {
        string key;
        {
            ostringstream buf;
            buf << "key " << get_fast_random(num_toplevel_keys);
            key = buf.str();
        }
        hash_map.erase(key);
    }

    void removeInner()
    {
        string key;
        {
            ostringstream buf;
            buf << "key " << get_fast_random(num_toplevel_keys);
            key = buf.str();
        }
        inner_map* inner = nullptr;
        lock_guard guard(global_lock);
        auto iter = hash_map.find(key);
        if (iter != hash_map.end()) {
            inner = iter->second;
            ZASSERT(inner);
        }
        if (!inner)
            return;
        for (size_t count = get_fast_random(max_nodes_to_remove); count--;) {
            string subkey;
            {
                ostringstream buf;
                buf << "subkey " << get_fast_random(num_inner_keys);
                subkey = buf.str();
            }
            inner->map.erase(subkey);
        }
    }

    void moveToGlobal()
    {
        string key;
        {
            ostringstream buf;
            buf << "key " << get_fast_random(num_toplevel_keys);
            key = buf.str();
        }
        auto iter = hash_map.find(key);
        if (iter == hash_map.end())
            return;
        inner_map* inner = iter->second;
        ZASSERT(inner);
        hash_map.erase(iter);
        lock_guard guard(global_lock);
        global_hash_map[key] = inner;
    }

    void moveFromGlobal()
    {
        string key;
        {
            ostringstream buf;
            buf << "key " << get_fast_random(num_toplevel_keys);
            key = buf.str();
        }
        inner_map* inner;
        {
            lock_guard guard(global_lock);
            auto iter = global_hash_map.find(key);
            if (iter == hash_map.end())
                return;
            inner = iter->second;
            ZASSERT(inner);
            global_hash_map.erase(iter);
        }
        hash_map[key] = inner;
    }

    unordered_map<string, inner_map*> hash_map;
};

inline size_t repeats()
{
    size_t result = base_repeats;
    if (zgc_is_scribbling())
        result /= 2;
    if (zgc_is_verifying())
        result /= 2;
    zprintf("repeats = %zu\n", result);
    return result;
}

inline void thread_main()
{
    LocalTester localTester;
    for (size_t repeat = repeats(); repeat--;) {
        unsigned selection = get_fast_random(selection_range);
        if (selection < add_nodes_probability) {
            localTester.addNodes();
            continue;
        }
        selection -= add_nodes_probability;
        if (selection < remove_top_level_probability) {
            localTester.removeTopLevel();
            continue;
        }
        selection -= remove_top_level_probability;
        if (selection < remove_inner_probability) {
            localTester.removeInner();
            continue;
        }
        selection -= remove_inner_probability;
        if (selection < move_to_global_probability) {
            localTester.moveToGlobal();
            continue;
        }
        selection -= move_to_global_probability;
        ZASSERT(selection < move_from_global_probability);
        localTester.moveFromGlobal();
    }
    lock_guard guard(global_lock);
    num_threads_done++;
}

} // anonymous namespace

int main(int argc, char** argv)
{
    size_t num_deleted = 0;
    new thread([&] {
        for (;;) {
            inner_map* map = static_cast<inner_map*>(zgc_finq_wait(finalizer_queue));
            delete map;
            num_deleted++;
        }
    });
    
    vector<thread> threads;
    for (size_t index = num_threads; index--;)
        threads.push_back(thread(thread_main));

    while (num_threads_done < num_threads) {
        usleep(1000000);
        size_t num_toplevel_entries = 0;
        size_t num_inner_entries = 0;
        {
            lock_guard guard(global_lock);
            for (auto& pair : global_hash_map) {
                num_toplevel_entries++;
                num_inner_entries += pair.second->map.size();
            }
        }
        zprintf("num_toplevel_entries = %zu, num_inner_entries = %zu, num_deleted = %zu\n",
                num_toplevel_entries, num_inner_entries, num_deleted);
    }

    for (thread& t : threads)
        t.join();

    printf("Sukces!\n");
    return 0;
}

