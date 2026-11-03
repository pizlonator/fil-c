#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* Two defined functions and one declared-but-undefined callee, all in the
   same module: "test" calls "aFunc", which calls "helper" (declared, never
   defined, not mapped).  CompileModule aborts (SIGABRT):
   "Cannot find helper in global mapping". */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef i32Fn = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef helper = LLVMAddFunction(module, "helper", i32Fn);
    LLVMValueRef aFunc = LLVMAddFunction(module, "aFunc", i32Fn);
    LLVMValueRef fn = LLVMAddFunction(module, "test", i32Fn);

    LLVMBasicBlockRef aFuncEntry = LLVMAppendBasicBlockInContext(context, aFunc, "ae");
    LLVMBuilderRef builder2 = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder2, aFuncEntry);
    LLVMValueRef helperResult = LLVMBuildCall2(builder2, i32Fn, helper, NULL, 0, "");
    LLVMBuildRet(builder2, helperResult);

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef aFuncResult = LLVMBuildCall2(builder, i32Fn, aFunc, NULL, 0, "");
    LLVMBuildRet(builder, aFuncResult);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_CompileModule(jit, module);

    return 0;
}
