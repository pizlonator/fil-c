#ifndef LLVM_PASSES_REGISTERFILPASSES_H
#define LLVM_PASSES_REGISTERFILPASSES_H

namespace llvm {

class PassBuilder;
struct FilJITConfiguration;

void RegisterFilPasses(PassBuilder&, FilJITConfiguration* = nullptr);

} // namespace llvm

#endif // LLVM_PASSES_REGISTERFILPASSES_H
