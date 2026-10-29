#ifndef LLVM_TRANSFORMS_INSTRUMENTATION_FILPIZLONATOR_H
#define LLVM_TRANSFORMS_INSTRUMENTATION_FILPIZLONATOR_H

#include "llvm/IR/PassManager.h"

#include <unordered_map>
#include <string>

namespace llvm {

class Module;

struct FilPtrLiteral {
  void* ptr { nullptr };
  void* lower { nullptr };

  FilPtrLiteral() = default;
  
  template<typename T>
  FilPtrLiteral(const T& other):
    ptr(other.ptr),
    lower(other.lower) {
  }
};

struct FilJITConfiguration {
  std::unordered_map<std::string, FilPtrLiteral> GlobalMapping;
  std::unordered_map<void*, void*> PointerMapping; // maps ptr->lower
};

class FilPizlonatorPass : public PassInfoMixin<FilPizlonatorPass> {
  FilJITConfiguration* FJITC;
public:
  FilPizlonatorPass(FilJITConfiguration *FJITC = nullptr);
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
  static bool isRequired() { return true; }
};

} // namespace llvm

#endif /* LLVM_TRANSFORMS_INSTRUMENTATION_FILPIZLONATOR_H */

