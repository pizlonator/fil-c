// An exception in flight on each of two stacks: main unwinds past a destructor that
// switches to a coroutine, which throws and catches its own exception (like boost's
// forced_unwind) and switches back. Main's unwind must continue with its own exception.
#include <ucontext.h>
#include <cstdio>
#include <exception>
#include <stdexcept>

static ucontext_t main_ctx, co_ctx;
static char co_stack[256 * 1024];
static std::exception_ptr co_error;
static bool unwinding_co;
struct forced_unwind {};

static void co_main() {
    try {
        try {
            throw std::runtime_error("from coroutine");
        } catch (...) {
            co_error = std::current_exception();
        }
        swapcontext(&co_ctx, &main_ctx);
        if (unwinding_co)
            throw forced_unwind();
    } catch (forced_unwind const&) {
        std::puts("coroutine unwound");
    }
    swapcontext(&co_ctx, &main_ctx);
}

struct Coroutine {
    ~Coroutine() {
        unwinding_co = true;
        swapcontext(&main_ctx, &co_ctx);
    }
};

__attribute__((noinline)) static void run() {
    Coroutine co;
    swapcontext(&main_ctx, &co_ctx);
    std::rethrow_exception(co_error);
}

int main() {
    getcontext(&co_ctx);
    co_ctx.uc_stack.ss_sp = co_stack;
    co_ctx.uc_stack.ss_size = sizeof co_stack;
    co_ctx.uc_link = nullptr;
    makecontext(&co_ctx, co_main, 0);
    try {
        run();
    } catch (std::exception const& e) {
        std::printf("main caught: %s\n", e.what());
        return 0;
    }
    return 1;
}
