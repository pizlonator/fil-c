#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMSetLinkage(global, internal) followed by LLVMSetVisibility(global,
   hidden): GlobalValue::setVisibility asserts "local linkage requires default
   visibility" (llvm/include/llvm/IR/GlobalValue.h) unless the linkage is
   neither internal nor private.  Deliberate tripwire: the manifest's
   output-excludes greps for LLVM's assert text. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMValueRef global = LLVMAddGlobal(module,
                                        LLVMInt32TypeInContext(context), "g");
    LLVMSetLinkage(global, LLVMInternalLinkage);
    LLVMSetVisibility(global, LLVMHiddenVisibility);
    return 0;
}
