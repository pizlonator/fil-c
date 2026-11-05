#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* The module reads an external global "never_defined_global" that is neither
   defined in the module nor in the session's global mapping.  CompileModule's
   mapping pre-check rejects it before the FilPizlonator pass can run, so this
   is a Fil-C panic (exit 42) instead of the pass's assertion abort. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef never = LLVMAddGlobal(module, i32, "never_defined_global");
    LLVMValueRef fn = LLVMAddFunction(module, "test", LLVMFunctionType(i32, NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef value = LLVMBuildLoad2(builder, i32, never, "v");
    LLVMBuildRet(builder, value);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_CompileModule(jit, module);

    return 0;
}
