#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMSetInstructionCallConv with a calling convention that does not fit in
   the 10-bit CallingConvField.  CallBase::setCallingConv packs the id into
   the bitfield, which asserts "value is too big" (Bitfields.h).  This must
   panic in the wrapper instead of tripping that assertion. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef callee = LLVMAddFunction(module, "callee",
        LLVMFunctionType(i32, NULL, 0, false));
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(i32, NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef call = LLVMBuildCall2(builder,
        LLVMFunctionType(i32, NULL, 0, false), callee, NULL, 0, "c");
    LLVMSetInstructionCallConv(call, 999999);
    return 0;
}
