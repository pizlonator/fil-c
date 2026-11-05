#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildAnd over two float loads; the wrapper's semantic check rejects non-integer operand types (BinaryOperator::AssertOK would have aborted). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef flt = LLVMFloatTypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef slot_a = LLVMBuildAlloca(builder, flt, "slot_a");
    LLVMValueRef a = LLVMBuildLoad2(builder, flt, slot_a, "a");
    LLVMValueRef slot_b = LLVMBuildAlloca(builder, flt, "slot_b");
    LLVMValueRef b = LLVMBuildLoad2(builder, flt, slot_b, "b");
    LLVMBuildAnd(builder, a, b, "r");
    return 0;
}
