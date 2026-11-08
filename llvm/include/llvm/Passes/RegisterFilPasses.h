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

void RegisterFilPasses(PassBuilder&, FilJITConfiguration* = nullptr,
                       bool IsRust = false);

// The pipeline-start passes as a directly-applicable entry point (needed by
// frontends whose pass pipelines are not always built through a PassBuilder,
// e.g. rustc's -C no-prepopulate-passes modules).
//
// IsRust (i.e. the Fil-C rustc) disables the pre-pizlonation simplification
// and inlining block by default: it still miscompiles rustc-shaped IR in the
// mem::swap class (see the FilCAddPipelineStartPasses comment in
// RegisterFilPasses.cpp).  FILCRUST_FILCINLINE in the environment re-enables
// it for debugging.
void FilCAddPipelineStartPasses(ModulePassManager&, OptimizationLevel,
                                bool IsRust, FilJITConfiguration* = nullptr);

} // namespace llvm

#endif // LLVM_PASSES_REGISTERFILPASSES_H
