#pragma once

#include <stdio.h>
#include <stdlib.h>

#define ASSERT(exp) do {\
    if ((exp)) \
        break; \
    fprintf(stderr, "%s:%d: %s: assertion %s failed.\n", \
            __FILE__, __LINE__, __PRETTY_FUNCTION__, #exp); \
    abort(); \
} while (false)

void* stuff();

template<typename T>
struct Foo {
    Foo() = default;

    Foo(const T& x)
        : x(x)
    {
    }

    __attribute__((noinline)) void* bar()
    {
        void* result = stuff();
        asm volatile ("" : : : "memory");
        return result;
    }
    
    T x { };
};

void* thingy();



