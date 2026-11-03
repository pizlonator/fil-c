#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* Malformed IR: returning a float constant from a function whose return type
   is i32.  The builder accepts it; the shim's verifyModule check at
   CompileModule must panic with "filc llvm jit error" (exit 42), printing the
   verifier's "Function return type does not match operand type of return
   inst!" message. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef flt = LLVMFloatTypeInContext(context);
    LLVMTypeRef i32Fn = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", i32Fn);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMBuildRet(builder, LLVMConstReal(flt, 1.5));

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_CompileModule(jit, module);

    return 0;
}
