#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the calling-convention checks: CallingConv::MaxID
   (1023) is the boundary value and must stay legal on both setters, along
   with an ordinary convention. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(i32, NULL, 0, false));
    LLVMValueRef callee = LLVMAddFunction(module, "callee",
        LLVMFunctionType(i32, NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef call = LLVMBuildCall2(builder,
        LLVMFunctionType(i32, NULL, 0, false), callee, NULL, 0, "c");
    LLVMSetFunctionCallConv(fn, 1023);
    LLVMSetInstructionCallConv(call, 1023);
    if (LLVMGetFunctionCallConv(fn) != 1023 ||
        LLVMGetInstructionCallConv(call) != 1023) {
        fprintf(stderr, "bad: boundary calling convention did not stick\n");
        abort();
    }
    LLVMSetFunctionCallConv(fn, 0);
    LLVMSetInstructionCallConv(call, 0);
    printf("callconv ok\n");
    return 0;
}
