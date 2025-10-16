# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Fil-C is a memory-safe implementation of C and C++ created by Filip Pizlo at Epic Games. It provides full C/C++ compatibility while preventing all memory safety errors through a combination of concurrent garbage collection and invisible capabilities (InvisiCap system).

Key characteristics:
- **Memory Safety**: All pointers carry capabilities tracking bounds and type information
- **GIMSO Principle**: "Garbage In, Memory Safety Out" - no unsafe escape hatches
- **Garbage Collection**: Uses FUGC (Fil's Unbelievable Garbage Collector) - concurrent, non-moving GC
- **Platform**: Currently Linux/X86_64 only
- **Performance**: 1.5x-5x slower than standard C (actively being optimized)

## Build System

### Initial Setup
```bash
# For source builds (primary development workflow)
./setup_gits.sh     # Clone/setup all required repositories

# Build options (choose one):
./build_all.sh                    # Basic build with musl
./build_all_glibc.sh              # Use glibc instead of musl

# For binary releases
./setup.sh
```

### Core Build Components (Essential)
- `build_base.sh` - LLVM/Clang compiler
- `build_runtime.sh` - Fil-C runtime (libpas)
- `build_yolomusl.sh` / `build_usermusl.sh` - musl libc implementations (both essential)
- `build_cxx.sh` - C++ standard library (essential)

### Alternative Libc (if using glibc)
- `build_yolo_glibc.sh` / `build_user_glibc.sh` - glibc implementations

### Quick Builds
- `build_all_fast.sh` - Essential components only
- `build_all_slow.sh` - All ported applications

## Testing

### Test Runner
```bash
filc/run-tests                    # Run all tests
filc/run-tests --filter regex     # Run tests matching pattern
filc/run-tests --test testname    # Run specific test
filc/run-tests --verbose          # Verbose output
filc/run-tests --no-run           # Compile only, don't run
```

### Test Structure
- Tests live in `filc/tests/*/` directories
- Each test has a `manifest` YAML file defining expectations
- Test runner generates build scripts and runs multiple configurations
- Configurations include: default, scribble (verification), STW (stop-the-world), release

### Running Individual Tests
Tests are compiled to `filc/test-output/testname/` with scripts:
- `compile.sh` - Build the test
- `justRun.sh` - Run with debug runtime
- `subRun*.sh` - Run with different GC configurations

## Development Workflow

### Using Fil-C Compiler
```bash
# Basic compilation
build/bin/clang -o program program.c -g -O

# C++ compilation  
build/bin/clang++ -o program program.cpp -g -O -std=c++20

# Must use -g for meaningful error messages
# Must use -O with -g to avoid compiler crashes
```

### Key Directories

**Compiler Implementation:**
- `llvm/lib/Transforms/Instrumentation/FilPizlonator.cpp` - Main LLVM pass implementing Fil-C transformations

**Runtime System:**
- `libpas/src/libpas/filc_runtime.{h,c}` and `libpas/src/libpas/filc_runtime_inlines.h` - Core runtime implementation
- `libpas/src/libpas/fugc.{h,c}` - Garbage collector
- `libpas/src/libpas/filc_start_program.c` - Program startup trampoline

**Fil-C Headers and Runtime:**
- `filc/include/stdfil.h` - Main Fil-C header
- `filc/src/` - Runtime components written in Fil-C
- `filc/main/` - Program entry point

**Libc Implementations:**
- **musl**: `projects/yolomusl/` (yolo libc), `projects/usermusl/` (user libc)
- **glibc**: `projects/yolo-glibc-2.40/` (yolo libc), `projects/user-glibc-2.40/` (user libc)

**Ported Applications:**
- `projects/*` directories contain memory-safe versions of various software packages
- `pizfix/` - Staging environment for compiled Fil-C programs

## Architecture Notes

### Two-Libc Architecture
Fil-C uses a "sandwich" architecture:
1. **Yolo libc** (bottom) - Minimally modified libc for runtime use
2. **Fil-C runtime** (middle) - Memory safety layer  
3. **User libc** (top) - Heavily modified libc that applications use

Both yolo and user libc implementations are essential - you cannot have a working Fil-C system without both.

### InvisiCap System
- Each pointer has an associated invisible capability
- Capabilities stored in auxiliary allocations, not visible to C address space
- Pointers in registers use two registers (pointer + capability)
- Enables full C compatibility while maintaining memory safety

### Safety Guarantees
- Out-of-bounds access detection (heap and stack)
- Use-after-free prevention
- Type confusion prevention
- Pointer race detection
- System call argument validation

## Common Issues

### ABI Slice Problem
- Fil-C code cannot link with regular C code
- Must port entire dependency chains to Fil-C
- This is fundamental due to incompatible pointer representations

### Debugging
- Always compile with `-g` for meaningful error messages
- Use `FUGC_STW=1` to force stop-the-world GC for debugging GC issues
- Use `FUGC_SCRIBBLE=1 FUGC_VERIFY=1` for memory corruption debugging
- Use `FILC_DUMP_SETUP=1` to verify environment variable settings (useful when using other debugging flags)

### GC Stress Testing
- Use `FUGC_MIN_THRESHOLD=0` to increase GC churn for stress testing (not for performance tuning)
- Default `FUGC_MIN_THRESHOLD` value is generally optimal for performance

### Performance Tuning
- Current bottlenecks: calling convention overhead, capability access patterns
- Use `-O2` or `-O3` for best performance

## Important Files for Development

- `Overview.md` - Detailed project description and layout
- `README.md` - Getting started guide
- `Manifesto.md` - Technical deep-dive into Fil-C design
- `invisicaps_by_example.md` - Examples of memory safety in action
- `gimso_semantics.md` - Formal semantics documentation