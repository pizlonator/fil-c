This document describes how [Fil-C](https://github.com/pizlonator/fil-c/blob/deluge/Manifesto.md)'s pointers work. Fil-C is totally memory safe even though it gives you almost all of the power you'd expect from C, including sophisticated uses of pointers. Fil-C pointers achieve memory safety using a capability model called *invisicaps*, which have these properties:

- Pointers appear to have their native size. Fil-C currently only works on 64-bit systems, so pointers appear to be 64-bit.

- Pointers always carry an invisible capability (invisicap) that describes what memory they are allowed to access, and what they can do to that memory.

The capability is invisible because other than via Fil-C reflection operations (compiler intrinsics and runtime functions unique to Fil-C), there is no way for a Fil-C program to see the capability. It's always there, but you cannot find it if you access memory.

Invisicaps offer a similar programming model to SoftBound and CHERI. However, unlike CHERI, which uses wide pointers (`sizeof(void*)` is 16 or more) to store the capability, Fil-C's capabilities are invisible in the address space and do not affect pointer size. And unlike SoftBound, Fil-C's capabilities have a complete story for atomics (you cannot break invisicap protections by racing, and atomic pointer loads/stores really are atomic). [This document is a work-in-progress description of Fil-C's semantics.](https://github.com/pizlonator/fil-c/blob/deluge/gimso_semantics.md)

I'll show you how that works with a bunch of example programs. In these programs I'll use the Fil-C header `<stdfil.h>`, which you only need to `#include` if you want to mess with Fil-C's guts.

All examples are compiled with `build/bin/clang -O -g` from my Fil-C working directory. If you want to try these examples yourself, [you can grab a recent binary release for Linux/X86_64](https://github.com/pizlonator/fil-c/releases).

# Simple Allocation

    #include <stdfil.h>
    #include <stdlib.h>
    
    int main()
    {
        char* p = malloc(16);
        zprintf("p = %P\n", p + 42);
        return 0;
    }

This simple program allocates a 16-byte object and prints the pointer to it using the special Fil-C `zprintf` function, which supports the Fil-C `%P` format specifier. This prints the invisible capability in addition to the pointer value. This program outputs:

    p = 0x7d249450427a,0x7d2494504250,0x7d2494504260

The format of this output is `ptr,lower,upper` - i.e. the first element is the pointer's value, the second element is the lower bound, and the last element is the upper bound. Since we added 42 to the pointer before printing it, the pointer is now above its upper bound.

Note that all Fil-C allocations are rounded up to 16 bytes and have 16 byte alignment. This is just for compatibility with other 64-bit C implementations, which also tend to have 16 byte minimum alignment.

# Out Of Bounds Access

    #include <stdfil.h>
    #include <stdlib.h>
    
    int main()
    {
        char* p = malloc(16);
        p[42] = 100;
        return 0;
    }

Because Fil-C pointers carry bounds, we can trivially detect out-of-bounds stores like this. This program outputs:

    filc safety error: cannot write pointer with ptr >= upper.
        pointer: 0x75a882b0427a,0x75a882b04250,0x75a882b04260
        expected 1 writable bytes.
    semantic origin:
        test2.c:7:11: main
    check scheduled at:
        test2.c:7:5: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [150645] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

The *semantic origin* is the place in the code that initiated the memory access that led to the safety check. Fil-C hoists checks so long as doing so doesn't break the program. The *check scheduled at* tells you where the check was hoisted to.

# Out Of Bounds But In Bounds

    #include <stdio.h>
    #include <stdlib.h>
    
    int main()
    {
        char* x = malloc(100);
        char* y = malloc(100);
        x[y - x] = '!';
        printf("*y = %c\n", *y);
        return 0;
    }

This program writes out-of-bounds of `x`, but in-bounds to `y`. Fil-C doesn't allow this, because the pointer being used to do the memory access (`x`) is out-of-bounds of its own capability.

    filc safety error: cannot write pointer with ptr >= upper.
        pointer: 0x7e8af1934190,0x7e8af1934110,0x7e8af1934180
        expected 1 writable bytes.
    semantic origin:
        test21.c:8:14: main
    check scheduled at:
        test21.c:8:14: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [715478] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

Note that by contrast, Yolo-C (i.e. not Fil-C) allows this and it works reliably:

    *y = !

Also, tag-based approaches to catching bugs in C code, like address sanitizer, do not catch this error and allow this program just fine (I'm using `gcc -O -g -fsanitize=address`):

    *y = !

This is a great example of Fil-C enforcing memory safety (the out-of-bounds access is not allowed based on `x`'s capability) and other approaches failing to enforce memory safety (asan allows this because the address that `x + (y - x)` points to happens to be a live address). It's important for memory safe languages to prevent this from happening, since attackers like to use invalid indices to array accesses to write to other objects in the heap. You can do that with asan, valgrind, and other safety approaches for C. You cannot do that in Fil-C, because Fil-C is memory safe.

# Overflowing Out Of Bounds

    #include <stdlib.h>
    #include <inttypes.h>
    
    int main()
    {
        char* p = malloc(16);
        p -= (uintptr_t)p;
        p += UINT_MAX;
        *(int*)p = 42;
        return 0;
    }

This program is quite clever, and if Fil-C's bounds checks were implemented carelessly, this access would bypass Fil-C's checks. Here's why:

- `UINTPTR_MAX` is greater than the lower bounds of any allocation. So, the lower bounds check will succeed.

- `UINTPTR_MAX + sizeof(int) = 3` because it overflows. Therefore, it's below the upper bounds of any allocation. So, the upper bounds check will succeed *if we had implemented it this way*.

Fil-C implements upper bounds checks using tricks to make sure that the overflow is caught. So, this program panics:

    filc safety error: cannot write pointer with ptr >= upper.
        pointer: 0xffffffffffffffff,0x79e07ab04250,0x79e07ab04260
        expected 4 writable bytes.
    semantic origin:
        test40.c:9:14: main
    check scheduled at:
        test40.c:8:7: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [1323066] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

Fil-C uses three different approaches to implementing the upper bounds check so that the overflow isn't an issue. Let's assume that `P` is the pointer being checked, `S` is the size of the access, `lower` is the lower bounds, and `upper` is the upper bounds. Let's also assume we've already done a lower bounds check `P >= lower`. And, `S` is never large (it can fit in a 32-bit integer, but in practice we can expect it's much smaller, like `sizeof` some C type).

- Upper bounds check can be `P <= upper - S`, since the lowest address at which any object is allocated is larger than `S`. This is what the compiler usually emits.

- Upper bounds check can be `P < upper && P + S <= upper`. I use this in some places in the runtime.

- Upper bounds check can be `P < upper` if `S` is equal to the alignment we checked. Some accesses have to check alignment. The compiler is smart enough to notice these cases to emit a simpler check.

# Pointers Passed To Syscalls

    #include <string.h>
    #include <unistd.h>
    
    int main()
    {
        write(1, "hello\n", strlen("hello\n"));
        return 0;
    }

This simple program just prints:

    hello

But what if we pass a bad pointer to `write`?

    #include <string.h>
    #include <unistd.h>
    
    int main()
    {
        write(1, "hello\n" - 100, strlen("hello\n"));
        return 0;
    }

Now we get:

    filc safety error: cannot read pointer with ptr < lower (ptr = 0x61326de8ccec,0x61326de8cd50,0x61326de8cd58,global,readonly).
        <runtime>: zsys_write
        src/unistd/write.c:7:9: write
        test19.c:6:5: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [614516] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

Fil-C's lowest level API is the syscall layer it exposes to libc (Fil-C uses musl as its libc). Fil-C's syscall implementation enforces memory safety. Here, the `zsys_write` function in the runtime is failing because we passed an out-of-bounds pointer.

Here's another example:

    #include <string.h>
    #include <unistd.h>
    
    int main()
    {
        write(1, "hello\n", 100);
        return 0;
    }

This also fails:

    filc safety error: cannot read 100 bytes when upper - ptr = 8 (ptr = 0x55c0cffe2d50,0x55c0cffe2d50,0x55c0cffe2d58,global,readonly).
        <runtime>: zsys_write
        src/unistd/write.c:7:9: write
        test20.c:6:5: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [614640] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

Note that Fil-C capabilities always have a multiple of 8 bytes in them, so the string constant's size is a full 8 bytes. But, the program is trying `write` 100 bytes, so this fails.

Fil-C provides wrappers for most of the syscalls that Linux provides (the goal is to wrap all of them, except a few that are not memory safe). Fil-C checks that any pointers passed to syscalls have the right capability for what the syscall will do to that pointer. Fil-C disallows syscalls that would break memory safety entirely (like `vfork`).

# Pointers In Memory

    #include <stdfil.h>
    #include <stdlib.h>
    
    int main()
    {
        char* p = malloc(16);
        *(int**)p = malloc(4);
        **(int**)p = 42;
        zprintf("p = %P\n", p);
        zprintf("*p = %P\n", *(int**)p);
        zprintf("**p = %d\n", **(int**)p);
        return 0;
    }

Now let's look at what happens when a pointer is stored into memory. Fil-C has to track `malloc(4)`'s capability even though it's no longer local to the program. This program prints:

    p = 0x77dab4504250,0x77dab4504250,0x77dab4504260,aux=0x77dab4508130
    *p = 0x77dab4504270,0x77dab4504270,0x77dab4504280
    **p = 42

Note that the printout for `p` now has an extra field: `aux=0x77dab4508130`. This is because `p` now contains pointers with capabilities. The capability of `malloc(4)` is stored in the aux allocation associated with `p`. There is no way to get an in-bounds Fil-C pointer to an aux location because Fil-C never gives you capabilities to access auxes. The aux allocation stores the capabilities of any pointers stored into `p`.

# Type Confusion: Integer Then Pointer

    #include <stdfil.h>
    #include <stdlib.h>
    
    int main()
    {
        char* p = malloc(16);
        *(int*)p = 666;
        int *p2 = *(int**)p;
        zprintf("p2 = %P\n", p2);
        return 0;
    }

In this program, we first store an integer to memory, and then we load it as a pointer. This is allowed, but the resulting program has the null capability. This program prints:

    p2 = 0x29a,<null>

If we modify the program to try to access the pointer:

    #include <stdfil.h>
    #include <stdlib.h>
    
    int main()
    {
        char* p = malloc(16);
        *(int*)p = 666;
        int *p2 = *(int**)p;
        *p2 = 42;
        return 0;
    }

Then we get:

    filc safety error: cannot write pointer with null object.
        pointer: 0x29a,<null>
        expected 4 writable bytes.
    semantic origin:
        test5.c:9:9: main
    check scheduled at:
        test5.c:9:9: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [151596] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

Fil-C doesn't allow accessing pointers with null capabilities. Hence, Fil-C is allowing the first stage of this type confusion - you can use a pointer load to load from memory that does not have a pointer - but it doesn't let you do anything harmful after that. The resulting pointer just knows its integer value, but cannot be accessed.

# Type Confusion: Pointer Then Integer

    #include <stdfil.h>
    #include <stdlib.h>
    
    int main()
    {
        char* p = malloc(16);
        *(int**)p = malloc(4);
        zprintf("%d\n", *(int*)p);
        return 0;
    }

This program does the opposite kind of type confusion: we store a pointer to memory, then load it back as an integer. This is fine, and the program prints:

    707805808

Which happens to be the low 32 bits of the pointer.

# Type Confusion: Store Pointer, Overwrite With Integer, Load Pointer

    #include <stdfil.h>
    #include <stdlib.h>
    
    int main()
    {
        char* p = malloc(16);
        *(int**)p = malloc(4);
        *(int*)p = 42;
        zprintf("*(int**)p = %P\n", *(int**)p);
        return 0;
    }

Here, we first store a pointer to memory, then we overwrite the low 32 bits of that pointer with an integer, and then we load the pointer back. This prints:

    *(int**)p = 0x7cb50000002a,0x7cb571f04270,0x7cb571f04280

Notice that the pointer is below the lower bounds, because the low 32 bits of the pointer have 0x2a (i.e. 42). Now consider a version of the program that tries to access the resulting pointer:

    #include <stdfil.h>
    #include <stdlib.h>
    
    int main()
    {
        char* p = malloc(16);
        *(int**)p = malloc(4);
        *(int*)p = 42;
        **(int**)p = 666;
        return 0;
    }

This prints:

    filc safety error: cannot write pointer with ptr < lower.
        pointer: 0x73780000002a,0x737806f04270,0x737806f04280
        expected 4 writable bytes.
    semantic origin:
        test8.c:9:16: main
    check scheduled at:
        test8.c:9:16: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [152073] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

Since the pointer is below lower bounds due to the integer store, this fails with a safety error.

Note that this even works if we overwrite the entire object pointed to by `p`, like in this program:

    #include <stdfil.h>
    #include <stdlib.h>
    
    int main()
    {
        char* p = malloc(16);
        *(int**)p = malloc(4);
        unsigned i;
        for (i = 16; i--;)
            p[i] = 42;
        **(int**)p = 666;
        return 0;
    }

This prints:

    filc safety error: cannot write pointer with ptr >= upper.
        pointer: 0x2a2a2a2a2a2a2a2a,0x761282504270,0x761282504280
        expected 4 writable bytes.
    semantic origin:
        test8b.c:11:16: main
    check scheduled at:
        test8b.c:11:16: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [470643] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

Notice how the entire pointer value is overwritten by 0x2a (i.e. 42), but the capability is totally intact. This is because the capability is not stored at any addresses that are accessible to the Fil-C program. So, when the pointer is loaded back, we get a pointer value full of 42 (because the Fil-C program overwrite that value with 42's) and the original capability (because storing integers into a memory location doesn't overwrite the invisible capability for that location).

# Type Confusion: Int And Float

    #include <stdio.h>
    
    int main()
    {
        int x = 666;
        printf("%e\n", *(float*)&x);
        return 0;
    }

Fil-C doesn't try to prevent type confusion between non-pointer types, like int versus float. This program is allowed, and prints:

    9.332648e-43

This means that you can use unions while violating the active union member rule in Fil-C.

    #include <stdio.h>
    #include <stdlib.h>
    
    union u {
        int x;
        float y;
    };
    
    int main()
    {
        union u* p = malloc(sizeof(union u));
        p->x = 666;
        printf("%e\n", p->y);
        return 0;
    }

This also prints:

    9.332648e-43

# Sophisticated Unions

    #include <stdio.h>
    #include <stdlib.h>
    
    union u {
        struct {
            int x;
            int y;
        };
        struct {
            const char* str1;
            const char* str2;
        };
        struct {
            double a;
            double b;
        };
    };
    
    int main()
    {
        union u* p = malloc(sizeof(union u));
        p->x = 1;
        p->y = 2;
        printf("(1) x = %d, y = %d, str1 = %p, str2 = %p, a = %le, b = %le\n",
               p->x, p->y, p->str1, p->str2, p->a, p->b);
        p->str1 = "hello";
        p->str2 = "world";
        printf("(2) x = %d, y = %d, str1 = %p, str2 = %p, a = %le, b = %le\n",
               p->x, p->y, p->str1, p->str2, p->a, p->b);
        p->a = 1.5;
        p->b = 2.5;
        printf("(3) x = %d, y = %d, str1 = %p, str2 = %p, a = %le, b = %le\n",
               p->x, p->y, p->str1, p->str2, p->a, p->b);
        return 0;
    }

This example shows more interesting type confusion using unions. This prints:

    (1) x = 1, y = 2, str1 = 0x200000001, str2 = 0, a = 4.243992e-314, b = 0.000000e+00
    (2) x = 1851820912, y = 23884, str1 = 0x5d4c6e608b70, str2 = 0x5d4c6e608b88, a = 5.068266e-310, b = 5.068266e-310
    (3) x = 0, y = 1073217536, str1 = 0x3ff8000000000000, str2 = 0x4004000000000000, a = 1.500000e+00, b = 2.500000e+00

Note that we're not printing the strings using `%s` but with `%p`, so we just see what the pointer's value. At the start, `p->str1` overlaps with `x` and `y` so it gets the value `0x200000001`, but with a null capability. `p->str2` stays NULL. Then, when we write string constants into `str1` and `str2`, we see the pointer's values printed in all of the fields. Finally, we see the double values in all of the fields.

If we changed the `%p` format specifiers to `%s`, then we get this output:

    filc safety error: cannot read pointer with null object.
        pointer: 0x200000001,<null>
        expected 1 bytes.
    semantic origin:
        src/string/memchr.c:9:14: memchr
    check scheduled at:
        src/string/memchr.c:9:14: memchr
        src/string/strnlen.c:6:18: strnlen
        src/stdio/vfprintf.c:600:12: printf_core
        src/stdio/vfprintf.c:690:13: vfprintf
        src/stdio/printf.c:9:8: printf
        test17.c:24:5: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [614299] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

We're crashing inside libc because `p->str1`'s value has no capability (since it's really the values of `p->x` and `p->y`), and so `printf` cannot print this string.

# Type Confusion: Function As Data

    #include <stdfil.h>
    #include <stdlib.h>
    
    static void foo(void)
    {
    }
    
    int main()
    {
        char* p = (char*)foo;
        zprintf("%d\n", (int)*p);
        return 0;
    }

This program illustrates another attempt to break Fil-C's protections: we are going to use a function as if it had data. This triggers a Fil-C error:

    filc safety error: cannot read pointer to special object.
        pointer: 0x63a6c0187200,aux=0x63a6c0187200,special(function),global,readonly
        expected 1 bytes.
    semantic origin:
        test10.c:11:26: main
    check scheduled at:
        test10.c:11:26: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [152314] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

This is because Fil-C function pointers are special capabilities that know that they have no accessible data in them. Here, the aux tells us the function's true address. So, the pointer that got printed out is saying:

- This is a function pointer.

- It really points at the function it should be pointing at.

- It's a global.

- It's readonly.

# Type Confusion: Data As Function

    #include <stdfil.h>
    #include <stdlib.h>
    
    int main()
    {
        void (*foo)(void) = malloc(16);
        foo();
        return 0;
    }

Now we're trying to call a pointer to data. This also triggers a safety error:

    filc safety error: cannot access pointer as function, object isn't even special (pts = 0x7d6f8a704250,0x7d6f8a704250,0x7d6f8a704260).
        test11.c:7:5: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [152457] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

# Offset Function Pointer

    #include <stdfil.h>
    #include <stdlib.h>
    
    static void foo(void)
    {
    }
    
    int main()
    {
        void (*my_foo)(void) = (void(*)(void))((char*)foo + 42);
        my_foo();
        return 0;
    }

We offset a function pointer and then try to call it. This also fails:

    filc safety error: cannot access pointer as function with ptr != aux (ptr = 0x6189fdb3a2aa,aux=0x6189fdb3a280,special(function),global,readonly).
        test12.c:11:5: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [152538] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

Because the function pointer no longer points at the function entrypoint indicated by the capability's aux, Fil-C rejects this function call.

# Use After Free

    #include <stdfil.h>
    #include <stdlib.h>
    
    int main()
    {
        int* p = malloc(4);
        free(p);
        *p = 42;
        return 0;
    }

Let's do a simple use after free bug. This is guaranteed to fail:

    filc safety error: cannot write pointer to free object.
        pointer: 0x792000904250,0x792000904250,0x792000904250,free
        expected 4 writable bytes.
    semantic origin:
        test13.c:8:8: main
    check scheduled at:
        test13.c:8:8: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [153367] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

The failure is guaranteed because `free()` doesn't actually free the memory; it just marks the capability free in-place. All subsequent accesses to that object then fail with this error. Note that the freed object appears to have an upper bound that is equal to the lower bound; this is due to an optimization (Fil-C doesn't actually do a distinct "is this free" check; free objects just have their upper bound clamped to force the bounds checks to fail).

Let's explore a bit about what I mean by the failure being guaranteed. Even if we try to groom the heap by allocating a lot of memory:

    #include <stdfil.h>
    #include <stdlib.h>
    
    int main()
    {
        int* p = malloc(4);
        free(p);
        unsigned count;
        for (count = 100000000; count--;)
            malloc(4);
        *p = 42;
        return 0;
    }

Fil-C will still ensure that this use after free gets a panic, because FUGC (Fil's Unbelievable Garbage Collector, a concurrent on-the-fly GC based on grey-stack Dijkstra) knows that `p` is still reachable. So, the object `p` points to is kept alive in a `free` state just to make sure that any use of it will definitely fail. To illustrate that this is really doing the allocations, I've run this program with `time`.

    filc safety error: cannot write pointer to free object.
        pointer: 0x79f016504250,0x79f016504250,0x79f016504250,free
        expected 4 writable bytes.
    semantic origin:
        test22.c:11:8: main
    check scheduled at:
        test22.c:11:8: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [715950] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)
    
    real    0m2.373s
    user    0m1.965s
    sys     0m0.140s

In cases where the freed object is referenced only from other objects in memory, FUGC will be able to free the object while still preserving the guaranteed use-after-free protection. That's because FUGC will repoint in-memory pointers to the freed objects to refer to the *free singleton* capability. Here's an example of that happening.

    #include <stdfil.h>
    #include <stdlib.h>
    
    int main()
    {
        int** p = malloc(sizeof(int*));
        *p = malloc(4);
        free(*p);
        unsigned count;
        for (count = 100000000; count--;)
            malloc(4);
        **p = 42;
        return 0;
    }

Note that extra level of indirection that makes it so that the freed object is not directly referenced from local variables. This allows FUGC to "move" the capability pointer to point to the free singleton.

    filc safety error: cannot write pointer to free object.
        pointer: 0x781b46504270,0x781b559b8fa8,0x781b559b8fa8,free,global,readonly
        expected 4 writable bytes.
    semantic origin:
        test23.c:12:9: main
    check scheduled at:
        test23.c:12:9: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [716090] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

Note that the error now reports that the pointer isn't even pointing at the same memory as the capability (it's 256593208 bytes away) and the capability is free, global, and readonly. That's because the pointer's capability is now the global free singleton, and the memory it originally pointed to (at address 0x781b46504270) has likely been reused. But even though the memory is reused, this pointer still deterministically fails when accessed, and so this pointer cannot be used to access the reused memory.

# Pointer Races

    #include <pthread.h>
    #include <stdlib.h>
    #include <stdio.h>
    
    static int* ptr;
    
    static void* thread_main(void* arg)
    {
        unsigned count;
        for (count = 10000000; count--;)
            ptr = malloc(4);
        return NULL;
    }
    
    int main()
    {
        pthread_t t;
        pthread_create(&t, NULL, thread_main, NULL);
    
        unsigned count;
        for (count = 1000000; count--;)
            ptr = malloc(4);
    
        asm volatile("" : : : "memory");
    
        printf("%d\n", *ptr);
        return 0;
    }

This example shows what happens when we have a pointer in shared memory (`ptr`) that isn't marked atomic, and we deliberately race on it. Non-atomic pointer accesses are really a pair of 64-bit accesses:

- A [monotonic](https://llvm.org/docs/Atomics.html#monotonic) access to the invisible capability.

- A non-atomic access to the pointer value.

Hence, it's possible that:

- We get tearing between the capability and the pointer's value.

- On some architectures (not X86_64), we get a totally bogus pointer value.

Both outcomes are safe in Fil-C, because a bogus pointer value, or a pointer value that doesn't match the capability, results in a pointer that traps on access.

This program does one more thing that's a little strange, but familiar to the Real C Programmers (TM): we emit a compiler fence using a dummy `asm` block that emits no code but clobbers memory. Fil-C disallows almost all inline assembly, but it does allow this idiom, because it's Awesome. We use it here to ensure that when the `printf` call at the end loads from `ptr`, it really loads from it rather than getting the value of the last `malloc` call as a result of load elimination.

In my tests, this program runs just fine about 99% of the time, and fails with a Fil-C panic about 1% of the time:

    filc safety error: cannot read pointer with ptr < lower.
        pointer: 0x719b13650370,0x719b13650390,0x719b136503a0
        expected 4 bytes.
    semantic origin:
        test25.c:26:20: main
    check scheduled at:
        test25.c:26:20: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [718033] filc panic: thwarted a futile attempt to violate memory safety.

Notice how Fil-C thinks that the pointer is below bounds; that's because we got a pointer-capability tear.

If we make one change to the program - add `_Atomic` to the signature of `ptr`:

    static int* _Atomic ptr;

Then the program works reliably every time. This is because `_Atomic` and `volatile` pointers in Fil-C use fancy lock-free algorithms to implement every pointer access. Fil-C supports all of clang's atomic intrinsics, `<stdatomic.h>`, and C++'s `std::atomic`. If you request a specific memory ordering for a pointer atomic operation, then you get *at least* monotonic ordering (because it has to at least be monotonic to ensure we get a valid capability).

# Laundering Pointers As Integers

    #include <stdio.h>
    #include <inttypes.h>
    
    int main()
    {
        const char* str = "hello";
        printf("%s\n", (const char*)((uintptr_t)str ^ 1));
        return 0;
    }

Sometimes C's pointer arithmetic is best expressed using integers. So, like in this program, we cast the pointer to `uintptr_t` (or similar), do some math to it, and then cast it back to pointer. Fil-C allows this and prints:

    ello

But this only works if the cast from int-to-ptr and ptr-to-int casts are local to one another and the compiler can unambiguously pick the original pointer's capability. For example, this doesn't work:

    #include <stdio.h>
    #include <inttypes.h>
    
    uintptr_t x;
    
    int main()
    {
        const char* str = "hello";
        x = (uintptr_t)str;
        asm volatile("" : : : "memory");
        printf("%s\n", (const char*)x);
        return 0;
    }

Here, we've made sure that the compiler cannot see the int-to-ptr cast as having any relationship to the ptr-to-int cast, since `x` is a global variable (so anyone could muck with it) and we have prevented any kind of load elimination (thanks to the compiler fence). So, this gets:

    filc safety error: cannot read pointer with null object.
        pointer: 0x60ac4d7f1cf0,<null>
        expected 1 bytes.
    semantic origin:
        src/string/strlen.c:8:9: strlen
    check scheduled at:
        src/string/strlen.c:8:9: strlen
        src/stdio/fputs.c:6:13: fputs
        src/stdio/puts.c:7:8: puts
        test28.c:11:5: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [722598] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

# Laundering Integers As Pointers

    #include <stdio.h>
    #include <inttypes.h>
    
    void* x;
    
    int main()
    {
        x = (void*)42;
        asm volatile("" : : : "memory");
        printf("%d\n", (int)x);
        return 0;
    }

Fil-C allows pointers to carry integer values. This is always fine and you can do the following things to such pointers:

- Cast them back to integers.

- Compare them.

- Pass them around.

But you cannot access them. For example, this doesn't work:

    #include <stdio.h>
    #include <inttypes.h>
    
    int* x;
    
    int main()
    {
        x = (int*)42;
        printf("%d\n", *x);
        return 0;
    }

This program panics:

    filc safety error: cannot read pointer with null object.
        pointer: 0x2a,<null>
        expected 4 bytes.
    semantic origin:
        test30.c:9:20: main
    check scheduled at:
        test30.c:9:20: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [722837] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

# Bad Linking: Not Enough Arguments

Fil-C's capability model extends to linker symbols. In Yolo-C, using a linker symbol really means getting a pointer to something allocated by the linker. In Fil-C, using a linker symbol really means getting a Fil-C pointer (so a pointer value and a capability) to something allocated by the Fil-C linker (Fil-C handles linking using a combination of the Fil-C runtime and a modified ELF linker). Additionally, function calls involve passing data along with capabilities, so a mismatch in function arguments is caught dynamically. Let's consider a simple example of this in action.

In one file we have:

    #include <stdio.h>
    
    void foo(int x)
    {
        printf("%d\n", x);
    }

And the other file:

    void foo(void);
    
    int main()
    {
        foo();
        return 0;
    }

Compiling and running this yields:

    filc safety error: argument size mismatch (actual = 0, expected = 8).
        test32a.c: foo
        test32b.c:5:5: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [1216773] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

We get this error because the call to `foo()` passed no arguments (and the module containing the call declared it as an extern function that takes no arguments), but the actual definition is a function that takes one argument.

# Bad Linking: Wrong Arguments (Expected Pointer, Passed Integer)

First file:

    #include <stdio.h>
    
    void foo(char* str)
    {
        printf("%s\n", str);
    }

Second file:

    void foo(int x);
    
    int main()
    {
        foo(666);
        return 0;
    }

This yields:

    filc safety error: cannot read pointer with null object.
        pointer: 0x29a,<null>
        expected 1 bytes.
    semantic origin:
        src/string/strlen.c:8:9: strlen
    check scheduled at:
        src/string/strlen.c:8:9: strlen
        src/stdio/fputs.c:6:13: fputs
        src/stdio/puts.c:7:8: puts
        test33a.c:5:5: foo
        test33b.c:5:5: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [1217134] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

# Bad Linking: Function As Data

First file:

    #include <stdio.h>
    
    void foo(void)
    {
        printf("witaj\n");
    }

Second file:

    #include <stdio.h>
    
    extern int foo;
    
    int main()
    {
        printf("%d\n", foo);
        return 0;
    }

We're trying to use `foo` as data, but it's really a function. This gives us:

    filc safety error: cannot read pointer to special object.
        pointer: 0x5d845ccaf210,aux=0x5d845ccaf210,special(function),global,readonly
        expected 4 bytes.
    semantic origin:
        test34b.c:7:20: main
    check scheduled at:
        test34b.c:7:20: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [1217401] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

# Bad Linking: Data As Function

First file:

    int foo = 666;

Second file:

    void foo(void);
    
    int main()
    {
        foo();
        return 0;
    }

Now we're going to try to use data as if it was a function. This gives us:

    filc safety error: cannot access pointer as function, object isn't even special (pts = 0x5ae542c2f068,0x5ae542c2f068,0x5ae542c2f070,global).
        test35b.c:5:5: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [1217542] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

# Bad Linking: Const Mixup

First file:

    const int x = 42;

Second file:

    extern int x;
    
    int main()
    {
        x = 666;
        return 0;
    }

Fil-C capabilities track whether an object is readonly or not. So, if you accidentally link to a const global as if it was non-const, and then write to it, you get:

    filc safety error: cannot write to read-only object.
        pointer: 0x61b2b5698d70,0x61b2b5698d70,0x61b2b5698d78,global,readonly
        expected 4 writable bytes.
    semantic origin:
        test39b.c:5:7: main
    check scheduled at:
        test39b.c:5:7: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [1218685] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

# Variadic Function Misuse: Not Enough Args

    #include <stdio.h>
    #include <stdarg.h>
    #include <stdlib.h>
    
    static void foo(int count, ...)
    {
        va_list list;
        va_start(list, count);
        printf("args:");
        while (count--)
            printf(" %d", va_arg(list, int));
        printf("\n");
    }
    
    int main()
    {
        setvbuf(stdout, NULL, _IONBF, 0); /* turn off buffered IO */
        foo(5, 1, 2, 3, 4, 5); /* good */
        foo(10, 666); /* bad */
        return 0;
    }

Another cause of safety issues in C is using `va_arg` when there are no more arguments to get. Fil-C catches this:

    args: 1 2 3 4 5
    args: 666filc safety error: cannot read pointer with ptr >= upper (ptr = 0x708f8f1047c0,0x708f8f1047b0,0x708f8f1047c0,readonly).
        test36.c:11:23: foo
        test36.c:19:5: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [1218006] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

# Variadic Function Misuse: Wrong Arg Type

    #include <stdio.h>
    #include <stdarg.h>
    #include <stdlib.h>
    
    static void foo(int count, ...)
    {
        va_list list;
        va_start(list, count);
        printf("args:");
        while (count--)
            printf(" %s", va_arg(list, const char*));
        printf("\n");
    }
    
    int main()
    {
        setvbuf(stdout, NULL, _IONBF, 0); /* turn off buffered IO */
        foo(5, 1, 2, 3, 4, 5); /* good */
        return 0;
    }

Another way to misuse `va_arg` is to try to get the wrong argument type. Fil-C allows `va_arg` to get a pointer when you really passed an integer, but then you get a pointer that lacks a capability. Using the capability then leads to a panic:

    args:filc safety error: cannot read pointer with null object.
        pointer: 0x1,<null>
        expected 1 bytes.
    semantic origin:
        src/string/memchr.c:9:14: memchr
    check scheduled at:
        src/string/memchr.c:9:14: memchr
        src/string/strnlen.c:6:18: strnlen
        src/stdio/vfprintf.c:600:12: printf_core
        src/stdio/vfprintf.c:690:13: vfprintf
        src/stdio/printf.c:9:8: printf
        test37.c:11:9: foo
        test37.c:18:5: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [1218108] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

# Variadic Function Misuse: va_list Escape

    #include <stdio.h>
    #include <stdarg.h>
    #include <stdlib.h>
    
    static void foo(va_list list, ...)
    {
        va_start(list, list);
    }
    
    int main()
    {
        setvbuf(stdout, NULL, _IONBF, 0); /* turn off buffered IO */
        va_list list;
        foo(list, 1, 2, 3, 4, 5);
        printf("args:");
        unsigned count = 5;
        while (count--)
            printf(" %d", va_arg(list, int));
        printf("\n");
        return 0;
    }

Escaping a `va_list` from the stack frame that has the arguments is super dangerous in Yolo-C. In Fil-C, this just works, because internally, the `va_list` has a pointer to a heap-allocated readonly object containing a snapshot of the arguments. Arguments are only heap-allocated for variadic functions. So, this program just works:

    args: 1 2 3 4 5

# Memory Leak

First file:

    #include <stdlib.h>
    
    void* allocate(void)
    {
        return malloc(1000);
    }

Second file:

    void allocate(void);
    
    int main()
    {
        unsigned count;
        for (count = 1000000000; count--;)
            allocate();
        return 0;
    }

This allocates and does not free 1,000,000,000,000 bytes. I don't have that much memory on my computer. This program works fine in Fil-C. Note that I've pulled the function doing the allocating into a separate compilation unit, so that the compiler can't just optimize the `malloc` call out (though the Fil-C compiler doesn't have that optimization right now).

Fil-C uses a concurrent garbage collector called FUGC (Fil's Unbelievable Garbage Collector). This test uses about 135% CPU according to `top`, meaning that FUGC is running 35% of the time. Memory usage stays between 5 MB and 7 MB, which is probably due to the lag between when these allocations happen and when FUGC can clean them up (this program is allocating very quickly).

FUGC will fix a large class of memory leaks for you. It won't fix all leaks - for example, if the leaked objects are registered in some global data structure, which makes them reachable according to FUGC. FUGC also means that you can write C or C++ code that is largely oblivious to memory management, like a GC'd language programmer normally would.

# Conclusion

This document is meant to give you a feeling for how Fil-C pointer work by showing some examples. This is not an exhaustive list of safety checks that Fil-C performs.
