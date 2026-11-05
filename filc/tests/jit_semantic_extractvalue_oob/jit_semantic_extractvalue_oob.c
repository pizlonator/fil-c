#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMBuildExtractValue with index 5 out of a loaded {i32,i32}.  The
   wrapper's aggregate/index check rejects the out-of-range index, so this is
   a Fil-C panic (exit 42) instead of the checkGEPType assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fields[2] = { i32, i32 };
    LLVMTypeRef st = LLVMStructTypeInContext(context, fields, 2, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef slot = LLVMBuildAlloca(builder, st, "slot");
    LLVMValueRef agg = LLVMBuildLoad2(builder, st, slot, "agg");
    LLVMBuildExtractValue(builder, agg, 5, "ev");

    return 0;
}
