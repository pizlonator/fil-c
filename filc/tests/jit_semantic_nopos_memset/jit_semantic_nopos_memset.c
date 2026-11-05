#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildMemSet on a builder whose insert position was cleared.
   IRBuilderBase::ClearInsertionPosition leaves the builder without an
   insert block, and the LLVM implementation of BuildMemSet derefs
   it with no assert, dying with SIGSEGV, so the wrapper's
   insert-position check must reject it: this is a Fil-C panic (exit
   42) instead of that crash.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i8 = LLVMInt8TypeInContext(context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef voidTy = LLVMVoidTypeInContext(context);
    LLVMTypeRef ptr = LLVMPointerTypeInContext(context, 0);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(voidTy, NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMClearInsertionPosition(builder);
    LLVMBuildMemSet(builder, LLVMConstNull(ptr),
                    LLVMConstInt(i8, 0, false),
                    LLVMConstInt(i32, 4, false), 0);

    return 0;
}
