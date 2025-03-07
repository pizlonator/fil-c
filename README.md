# Fil-C 0.668.3

This is a spare-time personal project that I'm doing for fun. It's called
Fil-C.

What is it? It's a totally memory-safe version of C and C++. All memory safety
errors are caught as Fil-C panics. Fil-C achieves this using a combination of
real-time garbage collection and invisible capabilities (each pointer in memory
has a corresponding capability, not visible to the C address space). Every
fundamental C operation (as seen in LLVM IR) is checked against the capability.
Fil-C has no `unsafe` escape hatch of any kind.

## License

The compiler (clang + LLVM) is covered by LLVM-LICENSE.txt. The runtime is
covered by PAS-LICENSE.txt (see libpas/LICENSE.txt in the source distribution).
The libc is covered by MUSL-LICENSE.txt (see yolomusl/COPYRIGHT and
usermusl/COPYRIGHT in the source distribution). The libc++/libc++abi are
covered by LLVM-LICENSE.txt.

You can fetch the compiler, runtime, libc++/libc++abi source from
https://github.com/pizlonator/llvm-project-deluge.

You can fetch musl source from https://github.com/pizlonator/deluded-musl
(there are two of them - the yolomusl branch for the libc that sits below the
runtime and the usermusl branch for the libc that sits above the runtime).

## Requirements

Fil-C only works on Linux/X86_64.

Previous versions worked on Darwin/ARM64 and FreeBSD, but now I'm focusing just
on Linux/X86_64 because it allows me to do a more faithful job of implementing
libc. There's nothing fundamentally stopping Fil-C from working on ARM or OSes
other than Linux.

## Getting Started

If you downloaded Fil-C binaries, run:

    ./setup.sh

If you downloaded Fil-C source, run:

    ./setup_gits.sh
    ./build_all.sh

Then you'll be able to use Fil-C from within this directory. There is no way to
"install" it, because that would be a crazy thing to do for such an
experimental piece of software!

## Things That Work

Included if you build from source using build_all.sh:

- Memory-safe SSH client and server.

- Memory-safe curl.

- Memory-safe libz and openssl.

- Memory-safe libc (based on musl) and libc++ (based on LLVM's libc++).

- A version of clang that you can use to compile C or C++ programs.

The binary distribution only includes clang, libc, and libc++.

Fil-C catches all of the stuff that makes memory safety in C hard, like:

- Out-of-bounds on the heap or stack.

- Use-after free (also heap or stack).

- Type confusion between pointers and non-pointers.

- Type errors arising from linking.

- Type errors arising from misuse of va_lists.

- Pointer races.

- System calls. All buffers passed to system calls are checked for bounds and
  type.

- Lots of other stuff.

Fil-C comes with a reasonably complete POSIX libc and even supports tricky
features like threads, signal handling, `mmap`/`munmap`, `longjmp`/`setjmp`,
and C++ exceptions.

## Things That Don't Work

Fil-C is not a complete product. Lots of stuff isn't done!

- Fil-C doesn't fully address UB not related to memory, yet. For example,
  dividing by zero may take the compiler down a weird path.

- I'm still working on performance. In good cases, it's 1.5x slower than normal
  C. In bad cases, it's 5x slower. Lots of work remains to fix those bad cases.

- Probably other stuff, too!

These are all things that can be fixed. They just haven't been, yet.

## Learn More

The best write-up of Fil-C is
[the manifesto](https://github.com/pizlonator/llvm-project-deluge/blob/deluge/Manifesto.md).

You can also e-mail me: pizlo@mac.com

And follow my rants on [Twitter](https://x.com/filpizlo).

I don't have a bug database or anything like that, so bothering me on Twitter
or over e-mail is your best bet, probably. And I might ignore you anyway.

No guarantees!

