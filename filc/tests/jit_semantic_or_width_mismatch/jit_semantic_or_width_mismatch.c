#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMBuildOr over an i32 load and an i64 load.  The wrapper's semantic
   check rejects operands of differing types, so this is a Fil-C panic (exit
   42) instead of the "two operands of differing type" assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64 = LLVMInt64TypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef slot32 = LLVMBuildAlloca(builder, i32, "slot32");
    LLVMValueRef slot64 = LLVMBuildAlloca(builder, i64, "slot64");
    LLVMValueRef lw32 = LLVMBuildLoad2(builder, i32, slot32, "v32");
    LLVMValueRef lw64 = LLVMBuildLoad2(builder, i64, slot64, "v64");
    LLVMBuildOr(builder, lw32, lw64, "or");

    return 0;
}
