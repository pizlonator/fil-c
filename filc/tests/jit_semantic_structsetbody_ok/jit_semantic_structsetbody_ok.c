#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMStructSetBody on an opaque named struct is legal (the first set).  The
   global gets an initializer and the module gets a function so that it
   compiles. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef named = LLVMStructCreateNamed(context, "named");
    LLVMTypeRef elements[1] = { i32 };
    LLVMStructSetBody(named, elements, 1, false);
    LLVMValueRef global = LLVMAddGlobal(module, named, "g");
    LLVMSetInitializer(global, LLVMConstNull(named));
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(i32, NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMBuildRet(builder, LLVMConstInt(i32, 42, false));

    if (LLVMTypeIsSized(named) != 1) {
        fprintf(stderr, "bad: struct should be sized now\n");
        abort();
    }

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    if (!FilLLVMJIT_GetPointerToGlobal(result, "test")) {
        fprintf(stderr, "bad: no test pointer\n");
        abort();
    }

    printf("structsetbody ok\n");
    return 0;
}
