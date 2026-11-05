#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMGetInlineAsm(vfn, "xyz"). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef vfnTy = LLVMFunctionType(LLVMVoidTypeInContext(context),
                                         NULL, 0, false);
    LLVMGetInlineAsm(vfnTy, "ret", 3, "xyz", 3, false, false,
                     LLVMInlineAsmDialectATT, false);
    return 0;
}
