#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Malformed IR: a function whose entry basic block has no terminator (not
   even an unreachable).  The builder accepts it, but the shim must panic
   with "filc llvm jit error" (exit 42) at CompileModule: the verifier's
   "Basic Block in function 'test' does not have terminator!" message runs on
   the caller's module BEFORE the CloneModule that would otherwise abort in
   CloneFunctionBodyInto ("dyn_cast on a non-existent value", Casting.h).
   This manifest deliberately does NOT exclude the "filc llvm jit error"
   string: that string IS the pinned panic. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMAppendBasicBlockInContext(context, fn, "root");

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_CompileModule(jit, module);

    return 0;
}
