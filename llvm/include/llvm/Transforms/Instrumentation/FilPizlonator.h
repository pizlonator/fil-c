#ifndef LLVM_TRANSFORMS_INSTRUMENTATION_FILPIZLONATOR_H
#define LLVM_TRANSFORMS_INSTRUMENTATION_FILPIZLONATOR_H

// Copyright (c) 2026 Filip Pizlo.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// This file is a part of Fil-C. https://fil-c.org/

#include "llvm/IR/PassManager.h"
#include "llvm/Support/CommandLine.h"

namespace llvm {

// Command-line switch enabling the Fil-C JIT lazy-extern mode. Defined in
// FilPizlonator.cpp. When true, FilPizlonatorPass emits lazily-resolved
// getters for extern functions (via filc_llvm_jit_resolve) and lowers the
// zexternptr idiom (via filc_llvm_jit_resolve_raw_pointer). Normal static
// compilation leaves this false and is completely unaffected. The
// lazily-resolved cache slots are accessed with target-gated atomics (a
// single 16-byte Acquire/Release i128 access on x86-64, a single 8-byte
// Acquire/Release lane plus symmetric-flight reconstruction elsewhere);
// see the atomic-cache helpers in FilPizlonator.cpp.
extern cl::opt<bool> FilCJitMode;

class Module;

class FilPizlonatorPass : public PassInfoMixin<FilPizlonatorPass> {
  bool JitMode = false;
public:
  FilPizlonatorPass() = default;
  explicit FilPizlonatorPass(bool JitMode) : JitMode(JitMode) {}
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
  static bool isRequired() { return true; }
};

} // namespace llvm

#endif /* LLVM_TRANSFORMS_INSTRUMENTATION_FILPIZLONATOR_H */
