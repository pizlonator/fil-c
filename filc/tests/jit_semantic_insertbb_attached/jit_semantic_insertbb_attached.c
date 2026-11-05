#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMInsertExistingBasicBlockAfterInsertBlock with a basic block that
   already belongs to a function.  The insert moves the block into the
   builder's function's block list, and BasicBlock::insertInto
   (BasicBlock.cpp) asserts "!Parent && "Already has a parent"" (through the
   list traits' "!V->getParent() && "Value already in a container!!"" in
   SymbolTableListTraitsImpl.h) for an attached block, so the wrapper's parent
   check must reject this: this is a Fil-C panic (exit 42) instead of that
   assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fn_ty = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn_a = LLVMAddFunction(module, "test", fn_ty);
    LLVMValueRef fn_b = LLVMAddFunction(module, "test2", fn_ty);
    LLVMBasicBlockRef entry_a = LLVMAppendBasicBlockInContext(context, fn_a, "rootA");
    LLVMBasicBlockRef entry_b = LLVMAppendBasicBlockInContext(context, fn_b, "rootB");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry_b);
    LLVMInsertExistingBasicBlockAfterInsertBlock(builder, entry_a);

    return 0;
}
