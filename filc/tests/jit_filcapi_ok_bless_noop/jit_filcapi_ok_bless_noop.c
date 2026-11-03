#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* FilLLVMJIT_BlessPointer records a pointer mapping that nothing consumes
   yet (the zblessedptr intrinsic is an unimplemented TODO), so blessing a
   host pointer and then compiling and running an unrelated normal module
   must work end to end. */

static int global = 5;

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef i32Fn = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", i32Fn);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMBuildRet(builder, LLVMConstInt(i32, 7, false));

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_BlessPointer(jit, &global);

    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(void) = FilLLVMJIT_GetPointerToGlobal(result, "test");
    if (!test || test() != 7) {
        fprintf(stderr, "bad: wrong result\n");
        abort();
    }

    printf("bless noop ok\n");
    return 0;
}
