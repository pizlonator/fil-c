#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstantPtrAuth(ptr, i32-key, i64-disc, ptr): LLVM's exact rule, so
   the LLVM-legal shape goes through.  The context is kept reachable until
   exit: ~LLVMContextImpl never tears down the ConstantPtrAuths uniquing map
   (an LLVM bug, probed with the bare C API), so letting the runtime dispose
   the context at exit while a ptrauth constant is still in it trips the
   "Uses remain when a value is destroyed!" assert in ~Value. */

LLVMContextRef keep_context;

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64 = LLVMInt64TypeInContext(context);
    LLVMTypeRef ptrTy = LLVMPointerTypeInContext(context, 0);
    LLVMValueRef auth = LLVMConstantPtrAuth(
        LLVMConstNull(ptrTy), LLVMConstInt(i32, 0, false),
        LLVMConstInt(i64, 0, false), LLVMConstNull(ptrTy));
    if (!auth) {
        fprintf(stderr, "bad: no ptrauth constant\n");
        abort();
    }
    keep_context = context;
    printf("ptrauth ok\n");
    return 0;
}
