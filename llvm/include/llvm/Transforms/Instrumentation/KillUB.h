#ifndef LLVM_TRANSFORMS_INSTRUMENTATION_KILLUB_H
#define LLVM_TRANSFORMS_INSTRUMENTATION_KILLUB_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class Module;

class KillUBPass : public PassInfoMixin<KillUBPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
  static bool isRequired() { return true; }
};

} // namespace llvm

#endif /* LLVM_TRANSFORMS_INSTRUMENTATION_KILLUB_H */

