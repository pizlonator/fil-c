//===- RegisterFilPasses.h - Fil-C pass pipeline registration --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// (c) 2026 Filip Pizlo. All Rights Reserved.
//
// This file is a part of Fil-C. https://fil-c.org/
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_PASSES_REGISTERFILPASSES_H
#define LLVM_PASSES_REGISTERFILPASSES_H

// ModulePassManager is a using-alias (of PassManager<Module>) in LLVM's
// PassManager.h, so it cannot be forward-declared as a class; include the
// header instead.
#include "llvm/IR/PassManager.h"

namespace llvm {

class PassBuilder;
class OptimizationLevel;
struct FilJITConfiguration;

// Registers the Fil-C pass pipeline on a PassBuilder: the pipeline-start
// extension point gets KillUB, the pre-pizlonation simplification and
// inlining (above O0), C++20 coroutine lowering and the FilPizlonator, and
// the optimizer-last extension point gets DeleteRedundantPollchecks.  This is
// how every Fil-C frontend pizlonates: clang/lib/CodeGen/BackendUtil.cpp,
// the Fil-C rustc (PassWrapper.cpp in the projeny patch), and the LLVM JIT
// shim (filc/llvmjit/filc_llvm_jit_shim.cpp) all register it, so every module
// gets the same pipeline-start block at every optimization level.
void RegisterFilPasses(PassBuilder&, FilJITConfiguration* = nullptr);

// The pipeline-start passes as a directly-applicable entry point (needed by
// frontends whose pass pipelines are not always built through a PassBuilder,
// e.g. rustc's -C no-prepopulate-passes modules).
void FilCAddPipelineStartPasses(ModulePassManager&, OptimizationLevel,
                                FilJITConfiguration* = nullptr);

} // namespace llvm

#endif // LLVM_PASSES_REGISTERFILPASSES_H
