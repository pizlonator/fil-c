# The Fil-C Manifesto: Garbage In, Memory Safety Out!

The C and C++ programming languages are wonderful. There is a ton of amazing code written in both of
them. But C and C++ are unsafe languages. Simple logic errors may result in an attacker controlling
where a pointer points and what is written into it, which leads to an easy path to exploitation. Lots
of other languages (Rust, Java, Haskell, even JavaScript) don't have this problem!

But I love C. And I love C++ almost as much. I grew up on them. It's such a joy for me to
use both of them! Therefore, in my spare time, I decided to make my own memory-safe C and C++. This is
a personal project and an expression of my love for C.

Fil-C introduces memory safety at the core of C and C++:
 
- All pointers carry a *capability*, which tracks the bounds and type of the pointed-to memory. Fil-C
  use a novel pointer encoding called *InvisiCap*, where each 64-bit pointer in memory has a
  corresponding capability, stored in an invisible part of the address space. The InvisiCap algorithm
  allows Fil-C to find the capability quickly whenever a pointer is loaded, and to replace the
  capability efficiently (and atomically if needed) when a pointer is stored. Pointers in flight (i.e.
  pointers being passed around in registers) utilize two registers; one for the pointer and one for
  the capability. The capability contains the lower and upper bounds, type information for special
  objects like functions, and everything needed to locate the invisible capabilities for any pointers
  stored in that object. Accessing memory causes a bounds check, and for pointer accesses, additional
  logic to access the capability.

- All allocations are *garbage collected* using
  [FUGC](https://github.com/pizlonator/fil-c/blob/deluge/libpas/src/libpas/fugc.c) (Fil's
  Unbelievable Garbage Collector). FUGC is a concurrent, real time, accurate garbage collector.
  Threads are never suspended for GC. Freeing an object causes its capability to have zero bounds,
  which prevents all future access. FUGC will redirect all object pointers to free objects to the free
  singleton object, which ensures that freed objects are definitely collected on the next cycle.
  Accessing a freed object before or after the next GC is guaranted to trap. Also, freeing objects is
  optional.

- The combination of InvisiCaps and FUGC means that it's not necessary to instrument or change
  `malloc` and `free` calls; the semantics are compatible with C. It's also not necessary to change
  unions. It's even possible to have int-ptr unions and to ping-pong between using the int and ptr
  members.

- The combination of InvisiCaps and FUGC means that pointer capabilities cannot be forged. Your
  program may have logic errors (bad casts, bogus pointer arithmetic, races, bad frees, use-after-free,
  whatever) but every pointer will remember the bounds and type of the thing it originated from. If
  you break that pointer's rules by trying to access out-of-bounds, or read an int as a pointer or
  vice-versa, or access a freed object, Fil-C will thwart your program's further execution.

- Fil-C supports tricky features like pthreads, signal handlers, mmap, C++ exceptions (which implies
  libunwind), and setjmp/longjmp. All of these features are memory-safe. Because Fil-C pointers
  support atomics, lock-free algorithms using pointers work just fine. Pointer races on pointers not
  marked `_Atomic` or `volatile` lead to Fil-C panics, at worst. It's even possible to allocate memory
  using `malloc` from within a signal handler (which is necessary because Fil-C heap-allocates stack
  allocations).

- Fil-C's protections are designed to be comprehensive. There's no escape hatch short of [delightful
  hacks that also break all memory-safe languages](https://blog.yossarian.net/2021/03/16/totally_safe_transmute-line-by-line).
  There's no `unsafe` keyword. Even data races result in memory-safe outcomes. Fil-C operates on the
  GIMSO principle: Garbage In, Memory Safety Out! No program accepted by the Fil-C compiler can
  possibly go on to escape out of the Fil-C type system. At worst, the program's execution will be
  thwarted at runtime by Fil-C.

Quick links:

- [Work-in-progress document describing Fil-C's GIMSO semantics](https://github.com/pizlonator/fil-c/blob/deluge/gimso_semantics.md).

- [Examples of Fil-C catching memory safety issues](https://github.com/pizlonator/fil-c/blob/deluge/invisicaps_by_example.md).

- [Releases (Linux/X86_64 binaries)](https://github.com/pizlonator/fil-c/releases).

Fil-C is already powerful enough to run a [memory-safe curl](https://github.com/pizlonator/deluded-curl-8.5.0)
and a [memory-safe OpenSSH (both client and server)](https://github.com/pizlonator/deluded-openssh-portable)
on top of a [memory-safe OpenSSL](https://github.com/pizlonator/deluded-openssl-3.2.0),
[memory-safe zlib](https://github.com/pizlonator/deluded-zlib-1.3),
[memory-safe pcre](https://github.com/pizlonator/pizlonated-pcre-8.39) (which required no changes),
[memory-safe CPython](https://github.com/pizlonator/pizlonated-cpython) (which required some changes
and even [found a bug](https://github.com/python/cpython/issues/118534)),
[memory-safe SQLite](https://github.com/pizlonator/pizlonated-sqlite),
[memory-safe libcxx and libcxxabi](https://github.com/pizlonator/fil-c/tree/deluge),
[memory-safe musl](https://github.com/pizlonator/deluded-musl) (Fil-C's current libc),
[memory-safe ICU](https://github.com/pizlonator/pizlonated-icu), as well as other programs (the list
is growing all the time).

Thanks to the flexibility of InvisiCaps, most programs compile and run with zero changes. Even
sophisticated programs like Lua and OpenSSH require zero code changes to work!

This works for me on my Linux X86_64 box:

    pizfix/bin/curl https://www.google.com/

as does this:

    pizfix/bin/ssh user@some.server.com

Where the `pizfix` is the Fil-C staging environment for *pizlonated* programs (programs that now
successfully compile with Fil-C). The only unsafety in Fil-C is in libpizlo (the runtime library),
which exposes all of the API that musl needs (low-level
[syscall and thread primitives](https://github.com/pizlonator/fil-c/blob/deluge/libpas/src/libpas/filc_runtime.c#2901),
which themselves perform comprehensive safety checking).

Fil-C is currently 1.5x slower than normal C in good cases, and about 4x slower in the worst cases.
I'm actively working on performance optimizations for Fil-C, so that 4x number will go down.

Note that Fil-C has previously used a two different capability models (MonoCaps and SideCaps), and a
different memory management model (isoheaps instead of FUGC). Those versions are obsolete, because
they had worse performance and worse compatibility (they required more code changes).

This document goes into the details of Fil-C and is organized as follows. First, I show you how to
use Fil-C. Then, I describe the remaining work to make Fil-C even faster. Then I conclude with a
description of the FUGC and InvisiCap algorithms.

## Using Fil-C

First I'll tell you how to download or build Fil-C and then I'll tell you how to use it.

### Using a Fil-C Release

You can download a Fil-C binary release from [https://github.com/pizlonator/fil-c/releases](https://github.com/pizlonator/fil-c/releases).

Then, in the filc-0.680-linux-x86_64 directory, run:

    ./setup.sh

This gives you a simple Fil-C setup with compiler, runtime, a musl-based libc and libc++.

### Building Fil-C From Source

Fil-C currently only works on Linux/X86_64. Upon getting Fil-C from
https://github.com/pizlonator/fil-c.git, and making sure you're on the `deluge` branch,
simply do:

    ./build_all.sh

This will build memory-safe musl, zlib, OpenSSL, curl, OpenSSH, pcre, and a bunch of other programs.
Now you can try to download something with the pizlonated curl, like maybe:

    pizfix/bin/curl https://www.google.com/

Or fire up a memory-safe sshd:

    sudo $PWD/pizfix/sbin/sshd -p 10022

And then even connect to it:

    pizfix/bin/ssh -p 10022 localhost

### Using Fil-C

Let's start with the basics. Fil-C works like any C compiler. Take this program:

    #include <stdio.h>
    int main() { printf("Hello!\n"); return 0; }

Say it's named hello.c. We can do:

    build/bin/clang -o hello hello.c -g -O

Note that without -g, the Fil-C runtime errors will not be as helpful, and if you don't add -O to
-g, the compiler will currently crash.

Let's quickly look at what happens with a broken program:

    #include <stdio.h>
    #include <stdlib.h>
    int main() {
        int* ptr = malloc(sizeof(int));
        printf("oob memory = %d\n", ptr[10]);
        return 0;
    }

Here's what happens when we compile and run this:

    $ build/bin/clang -o bad bad.c -O -g
    $ ./bad
    filc safety error: cannot read pointer with ptr >= upper.
        pointer: 0x72816c104278,0x72816c104250,0x72816c104260
        expected 4 bytes with ptr aligned to 4 bytes.
    semantic origin:
        bad.c:5:33: main
    check scheduled at:
        bad.c:5:33: main
        src/env/__libc_start_main.c:79:7: __libc_start_main
        <runtime>: start_program
    [3570029] filc panic: thwarted a futile attempt to violate memory safety.
    Trace/breakpoint trap (core dumped)

Fil-C thwarted this program's attempt to do something bad. Hooray!

The Fil-C version of clang works much like normal clang. It accepts C code and produces .o files
that your linker will understand. Some caveats:

- Fil-C currently relies on the staging environment (where all the pizlonated libraries, like OpenSSL
  and friends, live) to be in the llvm-project source directory.

- Fil-C currently relies on you *not* installing the compiler. You have to use it directly from the
  build directory created by the build_all.sh script.

- The [`llvm::FilPizlonatorPass`](https://github.com/pizlonator/fil-c/blob/deluge/llvm/lib/Transforms/Instrumentation/FilPizlonator.cpp)
  uses assert() as its error checking for now, so you must compile llvm with assertions enabled (the
  build_all.sh script does this).

Fil-C requires almost no changes to C or C++ code. Inline assembly is currently disallowed. Some configure
script jank has to change. Other than that, most code just compiles and works with zero changes!

## Making Fil-C Fast

The biggest impediment to using Fil-C in production is speed. Fil-C is currently about 1.5x-4x slower
than legacy C.

Why is it slow right now?

- The current calling convention and dynamic linking implementation is different from C, but relies
  on the C linker and C calling convention under the hood, resulting in doubling of both call and
  linking overheads.

- I'm still tuning how I implement InvisiCaps. It's a brand new capability model, and I haven't
  fully explored how to make it super fast.

- Likely other issues that I don't know about.

The plan to make Fil-C fast is to fix these issues. I believe that fixing these issues can get Fil-C
to be only 1.5x slower than C *in the worst cases*, with lots of programs being only 1.2x slower. But
it'll take some focused compiler/runtime/GC hacking to get there.

## InvisiCap

Fil-C uses a pointer representation where the pointer as seen by C code is the same size and
representation as in Yolo-C (i.e. normal, legacy C). Since Fil-C is focusing on 64-bit systems right
now, that means that pointers are 64-bit.

But there is an *invisible capability* associated with every pointer.

Pointers that are passed around in local data flow have an associated capability pointer passed around
(either in a separate register or a separate spill slot). That capability pointer points to the base
of the object, and at a negative address from base, there's a 16 byte `filc_object` struct that tells
the object's upper bounds and has an `aux` field that contains flags (for special cases like function
pointers) and may point to an *auxiliary allocation* that contains the capabilities associated with
pointers inside that object. Hence, the capability pointer is the lower bounds of the object, the
upper bounds can be retrieved from a negative offset from the capability pointer, and an auxiliary
allocation containing capabilities for pointers in that object can also be retrieved from a negative
offset from the capability pointer.

So, any pointer stored in the heap has its capability pointer stored in the auxiliary allocation
associated with the object that it was stored to.

This representation allows for:

- The illusion that pointers have their native size and representation.

- The ability to store an integer where you previously stored a pointer, and vice versa.

- The ability to load the integer bits of a pointer (in that case, you get the pointer's address but
  it's a capability-less integer so you cannot access it).

- The ability to load integers as pointers (in that case, you get a pointer that has no capability,
  and cannot be dereferenced).

- Super cheap non-pointer accesses, which only require a lower and upper bounds check. The lower
  bounds are indicated by the capability pointer itself, and the upper bounds are stored at a negative
  offset from the capability pointer.

Additional tricks are employed for atomic pointer accesses. Atomic pointer accesses result in the
auxiliary allocation having an *atomic box* for the atomically accessed pointer. Atomic boxes store
a 16-byte atomic tuple of capability and pointer.

Objects only get auxiliary allocations for capabilities if any field in the object has a capability.
So, for example, strings and frame buffers won't have auxiliary allocations. This means that the
space overhead of InvisiCaps is nowhere near 2x.

## Fil's Unbelievable Garbage Collector

[FUGC](https://github.com/pizlonator/fil-c/blob/deluge/libpas/src/libpas/fugc.c)
is a semi-novel algorithm. For those well-versed in concurrent GC design, it'll sound almost like
old hat - but the sort of old hat you enjoy wearing.

In GC mafia jargon, FUGC is a *parallel concurrent on-the-fly grey-stack Dijkstra accurate non-moving*
collector. Let's break that down:

- Parallel: marking and sweeping happen in multiple threads, in parallel. The more cores you have, the
  faster the collector finishes.

- Concurrent: marking and sweeping happen on some threads other than the *mutator* threads (i.e. your
  program's threads). Mutator threads don't have to stop and wait for the collector. The interaction
  between the collector thread and mutator threads is mostly non-blocking (locking is only used on
  allocation slow paths).

- On-the-fly: there is no global stop-the-world, but instead we use what some mafia members will call
  "soft handshakes" while other made men will call "ragged safepoints". In the language of civilians,
  this means that the GC may ask threads to do some work (like scan stack), but threads do this
  asynchronously, on their own time, without waiting for the collector or other threads. The only "pause"
  threads experience is the callback executed in response to the soft handshake, which does work bounded
  by that thread's stack height. That "pause" is usually shorter than the slowest path you might take
  through a typical `malloc` implementation.

- Grey-stack: the collector assumes it must rescan thread stacks to fixpoint. That is, GC starts with
  a soft handshake to scan stack, and then marks in a loop. If this
  loop runs out of work, then FUGC does another soft handshake. If that reveals more objects, then
  concurrent marking resumes. This prevents us from having a *load barrier* (no instrumentation runs
  when loading a pointer from the heap into a local variable). Only a *store barrier* is
  necessary, and that barrier is very simple. This fixpoint converges super quickly because all newly
  allocated objects during GC are pre-marked.

- Dijkstra: storing a pointer field in an object that's in the heap or in a global variable while FUGC
  is in its marking phase causes the newly pointed-to object to get marked. This is called a *Dijkstra
  barrier* and it is a kind of *store barrier*. Due to the grey stack, there is no load barrier like
  in the [classic Dijkstra collector](https://lamport.azurewebsites.net/pubs/garbage.pdf). The FUGC store
  barrier uses a compare-and-swap with relaxed memory ordering on the slowest path (if the GC is running
  and the object being stored was not already marked).

- Accurate: the GC accurately (aka precisely, aka exactly) finds all pointers to objects, nothing more,
  nothing less. `llvm::FilPizlonator` ensures that the runtime always knows where the root pointers are
  on the stack and in globals. The Fil-C runtime has a clever API and Ruby code generator for tracking
  pointers in low-level code that interacts with pizlonated code. All objects know where their outgoing
  pointers are - they can only be in the InvisiCap auxiliary allocation.

- Non-moving: the GC doesn't move objects. This makes concurrency easy to implement and avoids
  a lot of synchronization between mutator and collector. However, FUGC will "move" pointers to free
  objects (it will repoint the capability pointer to the free singleton so it doesn't have to mark the
  freed allocation).

This makes FUGC an *advancing wavefront* garbage collector. Advancing wavefront means that the
mutator cannot create new work for the collector by modifying the heap. Once an
object is marked, it'll stay marked for that GC cycle. It's also an *incremental update* collector, since
some objects that would have been live at the start of GC might get freed if they become free during the
collection cycle.

FUGC relies on *safepoints*, which comprise:

- *Pollchecks* emitted by the compiler. The `llvm::FilPizlonator` emits pollchecks often enough that only a
  bounded amount of progress is possible before a pollcheck happens. The fast path of a pollcheck is
  just a load-and-branch. The slow path runs a *pollcheck callback*, which does work for FUGC.

- Soft handshakes, which request that a pollcheck callback is run on all threads and then waits for
  this to happen.

- *Enter*/*exit* functionality. This is for allowing threads to block in syscalls or long-running
  runtime functions without executing pollchecks. Threads that are in the *exited* state will have
  pollcheck callbacks executed by the collector itself (when it does the soft handshake). The only
  way for a Fil-C program to block is either by looping while entered (which means executing a
  pollcheck at least once per loop iteration, often more) or by calling into the runtime and then
  exiting.

Safepointing is essential for supporting threading (Fil-C supports pthreads just fine) while avoiding
a large class of race conditions. For example, safepointing means that it's safe to load a pointer from
the heap and then use it; the GC cannot possibly delete that memory until the next pollcheck or exit.
So, the compiler and runtime just have to ensure that the pointer becomes tracked for stack scanning at
some point between when it's loaded and when the next pollcheck/exit happens, and only if the pointer is
still live at that point.

The safepointing functionality also supports *stop-the-world*, which is currently used to implement
`fork(2)` and for debugging FUGC (if you set the `FUGC_STW` environment variable to `1` then the
collector will stop the world and this is useful for triaging GC bugs; if the bug reproduces in STW
then it means it's not due to issues with the store barrier). The safepoint infrastructure also allows
safe signal delivery; Fil-C makes it possible to use signal handling in a practical way. Safepointing is
a common feature of virtual machines that support multiple threads and accurate garbage collection,
though usually, they are only used to stop the world rather than to request asynchronous activity from all
threads. See [here](https://foojay.io/today/the-inner-workings-of-safepoints/) for a write-up about
how OpenJDK does it. The Fil-C implementation is in [`filc_runtime.c`](https://github.com/pizlonator/fil-c/blob/deluge/libpas/src/libpas/filc_runtime.c).

Here's the basic flow of the FUGC collector loop:

1. Wait for the GC trigger.
2. Turn on the store barrier, then soft handshake with a no-op callback.
3. Turn on black allocation (new objects get allocated marked), then soft handshake with a callback
   that resets thread-local caches.
4. Mark global roots.
5. Soft handshake with a callback that requests stack scan and another reset of thread-local caches.
   If all collector mark stacks are empty after this, go to step 7.
6. Tracing: for each object in the mark stack, mark its outgoing references (which may grow the mark
   stack). Do this until the mark stack is empty. Then go to step 5.
7. Turn off the store barrier and prepare for sweeping, then soft handshake to reset thread-local
   caches again.
8. Perform the sweep. During the sweep, objects are allocated black if they happen to be allocated out
   of not-yet-swept pages, or white if they are allocated out of alraedy-swept pages.
9. Victory! Go back to step 1.

If you're familiar with the literature, FUGC is sort of like the DLG (Doligez-Leroy-Gonthier) collector
(published in [two](https://xavierleroy.org/publi/concurrent-gc.pdf)
[papers](http://moscova.inria.fr/~doligez/publications/doligez-gonthier-popl-1994.pdf) because they
had a serious bug in the first one), except it uses the Dijkstra barrier and a grey stack, which
simplifies everything but isn't as academically pure (FUGC fixpoints, theirs doesn't). I first came
up with the grey-stack Dijkstra approach when working on
[Fiji VM](http://www.filpizlo.com/papers/pizlo-eurosys2010-fijivm.pdf)'s CMR and
[Schism](http://www.filpizlo.com/papers/pizlo-pldi2010-schism.pdf) garbage collectors. The main
advantage of FUGC over DLG is that it has a simpler (cheaper) store barrier and it's a slightly more
intuitive algorithm. While the fixpoint seems like a disadvantage, in practice it converges after a few
iterations.

Additionally, FUGC relies on a sweeping algorithm based on bitvector SIMD. This makes sweeping insanely
fast compared to marking. This is made thanks to the
[Verse heap config](https://github.com/pizlonator/fil-c/blob/deluge/libpas/src/libpas/verse_heap.h)
that I added to
[libpas](https://github.com/WebKit/WebKit/blob/main/Source/bmalloc/libpas/Documentation.md). FUGC
typically spends <5% of its time sweeping.

## Conclusion

Fil-C is an open source project that I'm working on in my spare time. All of its components are
permissively-licensed open source. Fil-C's compiler bits are Apache2 while the runtime bits are BSD.

