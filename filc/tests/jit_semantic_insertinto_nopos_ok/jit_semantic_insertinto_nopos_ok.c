#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the LLVMInsertIntoBuilder detached-instruction
   check: a builder with NO insert position skips the insert entirely
   (IRBuilderDefaultInserter::InsertHelper only inserts when the insert
   point is valid), so inserting an already-parented instruction into one
   stays legal.
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
    LLVMValueRef inst = LLVMBuildAlloca(builder, i32, "s");
    /* A fresh builder has no insert position. */
    LLVMBuilderRef nopos = LLVMCreateBuilderInContext(context);
    LLVMInsertIntoBuilder(nopos, inst);
    LLVMBuildRetVoid(builder);

    printf("insertinto nopos ok\n");
    return 0;
}
