#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* Malformed IR: a call passes an i32 argument where the callee takes i64.
   The builder accepts it; the shim's verifyModule check at CompileModule must
   panic with "filc llvm jit error" (exit 42), printing the verifier's own
   "Call parameter type does not match function signature!" message. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64 = LLVMInt64TypeInContext(context);
    LLVMTypeRef params[1] = { i64 };
    LLVMTypeRef i64Fn = LLVMFunctionType(i64, params, 1, false);
    LLVMValueRef callee = LLVMAddFunction(module, "callee", i64Fn);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef arg = LLVMConstInt(i32, 1, false);
    LLVMBuildCall2(builder, i64Fn, callee, &arg, 1, "");
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_CompileModule(jit, module);

    return 0;
}
