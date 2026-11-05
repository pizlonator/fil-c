#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the LLVMInsertIntoBuilder detached-instruction
   check: an instruction detached by LLVMInstructionClone inserts fine into
   a positioned builder.
 */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef src = LLVMBuildAlloca(builder, i32, "s");
    LLVMValueRef clone = LLVMInstructionClone(src);
    LLVMInsertIntoBuilder(builder, clone);
    LLVMBuildRetVoid(builder);

    printf("insertinto ok\n");
    return 0;
}
