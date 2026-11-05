#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstAddrSpaceCast(null-ptr0, ptr0): no short-circuit on the constant path. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef ptr0 = LLVMPointerTypeInContext(context, 0);
    LLVMConstAddrSpaceCast(LLVMConstNull(ptr0), ptr0);
    return 0;
}
