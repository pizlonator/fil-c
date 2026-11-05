#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildMemSet with a non-power-of-two alignment.  The builder converts
   the alignment to a MaybeAlign, which asserts "Alignment is neither 0 nor a
   power of 2" (Alignment.h).  This must panic in the wrapper instead of
   tripping that assertion. */

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
    LLVMValueRef dst = LLVMBuildAlloca(builder, i32, "d");
    LLVMBuildMemSet(builder, dst, LLVMConstInt(i32, 0, false),
                    LLVMConstInt(LLVMInt64TypeInContext(context), 4, false), 3);
    return 0;
}
