#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMIntrinsicCopyOverloadedName2 of a non-overloaded intrinsic with
   parameter types.  The Copy2 native bottoms out in the same
   getIntrinsicNameImpl assert as the declaration getter ("This version of
   getName is for overloaded intrinsics only", Intrinsics.cpp).  This must
   panic in the wrapper instead of tripping that assertion. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    unsigned trap_id = LLVMLookupIntrinsicID("llvm.trap", 9);
    LLVMTypeRef params[1] = { LLVMInt64TypeInContext(context) };
    size_t length = 0;
    if (!trap_id || LLVMIntrinsicIsOverloaded(trap_id)) {
        fprintf(stderr, "bad: llvm.trap lookup failed\n");
        abort();
    }
    LLVMIntrinsicCopyOverloadedName2(module, trap_id, params, 1, &length);
    return 0;
}
