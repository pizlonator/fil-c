// Destructors that run during unwinding throw and catch exceptions of their own, nested
// two deep in three frames. Each outer unwind must resume with its own exception.
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>

static int inner_catches;

// Throw and catch inside a destructor, itself nested one level deeper.
struct Inner {
    ~Inner() {
        try { throw std::string("inner-inner"); } catch (const std::string&) { inner_catches++; }
    }
};

struct Cleanup {
    const char* name;
    ~Cleanup() noexcept {
        try {
            Inner i;
            throw std::logic_error(name);
        } catch (const std::logic_error&) {
            inner_catches++;
        }
    }
};

__attribute__((noinline)) static void level3() { Cleanup c{"level3"}; throw std::runtime_error("outer"); }
__attribute__((noinline)) static void level2() { Cleanup c{"level2"}; level3(); }
__attribute__((noinline)) static void level1() { Cleanup c{"level1"}; level2(); }

int main() {
    std::exception_ptr saved;
    try {
        level1();
    } catch (...) {
        saved = std::current_exception();
    }
    // Each of the three cleanups caught two exceptions of its own.
    if (inner_catches != 6) { std::printf("inner_catches=%d\n", inner_catches); return 1; }
    try {
        std::rethrow_exception(saved);
    } catch (const std::runtime_error& e) {
        std::printf("outer caught %s after %d inner catches\n", e.what(), inner_catches);
    }
    // A later, unrelated exception still unwinds normally.
    try { level3(); } catch (const std::runtime_error& e) { std::printf("again %s\n", e.what()); }
    return inner_catches == 8 ? 0 : 2;
}
