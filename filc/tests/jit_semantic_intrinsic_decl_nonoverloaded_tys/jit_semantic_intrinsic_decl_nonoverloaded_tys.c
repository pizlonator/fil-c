#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMGetIntrinsicDeclaration of a non-overloaded intrinsic with parameter
   types.  getIntrinsicNameImpl asserts "This version of getName is for
   overloaded intrinsics only" (Intrinsics.cpp) when Tys is passed for a
   non-overloaded id.  This must panic in the wrapper instead of tripping
   that assertion. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    unsigned trap_id = LLVMLookupIntrinsicID("llvm.trap", 9);
    LLVMTypeRef params[1] = { LLVMInt64TypeInContext(context) };
    if (!trap_id || LLVMIntrinsicIsOverloaded(trap_id)) {
        fprintf(stderr, "bad: llvm.trap lookup failed\n");
        abort();
    }
    LLVMGetIntrinsicDeclaration(module, trap_id, params, 1);
    return 0;
}
