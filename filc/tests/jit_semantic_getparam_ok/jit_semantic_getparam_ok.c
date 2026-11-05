#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the LLVMGetParam index check: in-range indices return
   the function's arguments with their declared types. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef params[2] = { LLVMInt32TypeInContext(context),
                              LLVMInt64TypeInContext(context) };
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), params, 2, false));
    LLVMValueRef p0 = LLVMGetParam(fn, 0);
    LLVMValueRef p1 = LLVMGetParam(fn, 1);
    if (!p0 || !LLVMIsAArgument(p0) ||
        LLVMGetTypeKind(LLVMTypeOf(p0)) != LLVMIntegerTypeKind ||
        LLVMGetIntTypeWidth(LLVMTypeOf(p0)) != 32) {
        fprintf(stderr, "bad: param 0 did not come back\n");
        abort();
    }
    if (!p1 || !LLVMIsAArgument(p1) ||
        LLVMGetIntTypeWidth(LLVMTypeOf(p1)) != 64) {
        fprintf(stderr, "bad: param 1 did not come back\n");
        abort();
    }
    printf("getparam ok\n");
    return 0;
}
