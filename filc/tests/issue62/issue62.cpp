#include <atomic>
#include <thread>

int g_dummy;
std::atomic<int*> g_atom = &g_dummy;

static constexpr int kNum = 512 * 1024;

static void Worker(int** p)
{
    for (int i = 0; i < kNum; ++i)
    {
        // Panics under FILC.
        int* pPrev = g_atom.exchange(p[i], std::memory_order_acq_rel);

        // // Works under FILC.
        // int* pPrev = g_atom.load(std::memory_order_relaxed);
        // while (!g_atom.compare_exchange_weak(pPrev, p[i], std::memory_order_acq_rel, std::memory_order_relaxed))
        // {
        // }

        *pPrev = 0;
    }
}

int main()
{
    int** p = new int*[2 * kNum]{};
    for (int i = 0; i < 2 * kNum; ++i)
        p[i] = new int{};

    std::thread worker1(Worker, p);
    std::thread worker2(Worker, p + kNum);
    worker1.join();
    worker2.join();

    for (int i = 0; i < 2 * kNum; ++i)
        delete p[i];
    delete[] p;

    printf("OK\n");
}
