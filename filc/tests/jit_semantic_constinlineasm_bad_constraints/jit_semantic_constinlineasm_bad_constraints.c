#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstInlineAsm(vfn, "xyz"). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef vfnTy = LLVMFunctionType(LLVMVoidTypeInContext(context),
                                         NULL, 0, false);
    LLVMConstInlineAsm(vfnTy, "ret", "xyz", false, false);
    return 0;
}
