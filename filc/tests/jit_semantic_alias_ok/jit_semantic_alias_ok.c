#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMAddAlias2 over a function and over a global variable, and
   LLVMAliasSetAliasee with an aliasee whose type matches the alias's, are
   legal.  Aliases must point at definitions, so the global gets an
   initializer and the function gets a body. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fn_ty = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fn_ty);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef ci32 = LLVMConstInt(i32, 42, false);
    LLVMBuildRet(builder, ci32);
    LLVMValueRef global = LLVMAddGlobal(module, i32, "g");
    LLVMSetInitializer(global, ci32);

    LLVMValueRef alias = LLVMAddAlias2(module, fn_ty, 0, fn, "alias");
    LLVMAliasSetAliasee(alias, fn);
    LLVMAddAlias2(module, i32, 0, global, "alias2");

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    if (!FilLLVMJIT_GetPointerToGlobal(result, "test")) {
        fprintf(stderr, "bad: no test pointer\n");
        abort();
    }

    printf("alias ok\n");
    return 0;
}
