#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* Even an external function declaration that the module never references
   must be present in the session's global mapping: the FilPizlonator pass
   visits every global value.  CompileModule aborts (SIGABRT):
   "Cannot find unused_decl in global mapping".  (The plan expected this
   construction to compile; the observed behavior is pinned instead.) */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef voidFn =
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", voidFn);
    LLVMAddFunction(module, "unused_decl", voidFn);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_CompileModule(jit, module);

    return 0;
}
