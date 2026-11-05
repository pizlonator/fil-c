#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* A branch in one function targeting a basic block of another function.  The
   wrapper's basic-block parent check rejects it at build time, so this is a
   Fil-C panic (exit 42) instead of the abort inside CompileModule's
   CloneModule ("Referenced value not in value map!"), which runs before any
   verifier.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef voidFn =
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", voidFn);
    LLVMValueRef other = LLVMAddFunction(module, "other", voidFn);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBasicBlockRef otherBB = LLVMAppendBasicBlockInContext(context, other, "ob");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMBuildBr(builder, otherBB);

    LLVMBuilderRef builder2 = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder2, otherBB);
    LLVMBuildRetVoid(builder2);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_CompileModule(jit, module);

    return 0;
}
