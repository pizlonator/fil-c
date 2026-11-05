#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* The module calls an external function "never_defined" that is neither
   defined in the module nor in the session's global mapping.  CompileModule's
   mapping pre-check rejects it before the FilPizlonator pass can run, so this
   is a Fil-C panic (exit 42) instead of the pass's "Cannot find never_defined
   in global mapping" assertion abort. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef i32Fn = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef never = LLVMAddFunction(module, "never_defined", i32Fn);
    LLVMValueRef fn = LLVMAddFunction(module, "test", i32Fn);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef result = LLVMBuildCall2(builder, i32Fn, never, NULL, 0, "");
    LLVMBuildRet(builder, result);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_CompileModule(jit, module);

    return 0;
}
