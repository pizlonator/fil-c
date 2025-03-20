/* The Computer Language Benchmarks Game
 * https://salsa.debian.org/benchmarksgame-team/benchmarksgame/
 *
 * contributed by Danial Klimkin (C++)
 * contributed by the Rust Project Developers (Rust)
 * contributed by TeXitoi (Rust)
 * contributed by Cristi Cobzarenco (Rust)
 * contributed by Matt Brubeck (Rust) 
 * contributed by Dmytro Ovdiienko
 *
 */

#include <atomic>
#include <numeric>
#include <algorithm>
#include <functional>
#include <iostream>
#include <thread>

struct Node
{
    Node *l, *r;

    int check() const
    {
        if (l)
            return l->check() + 1 + r->check();
        else
            return 1;
    }
};

Node* make(const int d)
{
    Node* root = new Node;
    if (d > 0)
    {
        root->l = make(d - 1);
        root->r = make(d - 1);
    }
    else
    {
        root->l = root->r = nullptr;
    }
    return root;
}

int run_parallel(unsigned depth, int iterations, unsigned int workers = std::thread::hardware_concurrency())
{
    std::vector<std::thread> threads;
    threads.reserve(workers);

    std::atomic_int counter = iterations;
    std::atomic_int output = 0;

    for(unsigned i = 0; i < workers; ++i) {
        threads.push_back(std::thread([&counter, depth, &output] {
            int checksum = 0;

            while(--counter >= 0) {
                Node* a     = make(depth);
                checksum    += a->check();
            }

            output += checksum;
        }));
    }
    
    for(unsigned i = 0; i < workers; ++i) {
        threads[i].join();
    }

    return output;
}

constexpr auto MIN_DEPTH     = 4;

int main(int argc, char* argv[])
{
    const int max_depth     = std::max(MIN_DEPTH + 2, (argc == 2 ? atoi(argv[1]) : 10));
    const int stretch_depth = max_depth + 1;

    // Alloc then dealloc stretchdepth tree.
    {
        Node* c = make(stretch_depth);
        std::cout << "stretch tree of depth " << stretch_depth << "\t "
                  << "check: " << c->check() << std::endl;
    }

    Node* long_lived_tree = make(max_depth);

    for (int d = MIN_DEPTH; d <= max_depth; d += 2)
    {
        const int iterations = 1 << (max_depth - d + MIN_DEPTH);
        auto const c = run_parallel(d, iterations);

        std::cout << iterations << "\t trees of depth " << d << "\t check: " << c << "\n";
    }

    std::cout << "long lived tree of depth " << max_depth << "\t "
              << "check: " << (long_lived_tree->check()) << "\n";

    return 0;
}
