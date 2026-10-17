#ifndef LLVM_TRANSFORMS_INSTRUMENTATION_FILPIZLONATOR_H
#define LLVM_TRANSFORMS_INSTRUMENTATION_FILPIZLONATOR_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class Module;

class FilPizlonatorPass : public PassInfoMixin<FilPizlonatorPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
  static bool isRequired() { return true; }
};

class FilCLoopBoundsEliminationPass : public PassInfoMixin<FilCLoopBoundsEliminationPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
  static bool isRequired() { return true; }
};

} // namespace llvm

#endif /* LLVM_TRANSFORMS_INSTRUMENTATION_FILPIZLONATOR_H */

