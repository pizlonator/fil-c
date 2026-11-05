#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMAppendExistingBasicBlock with a basic block that already belongs to a
   function.  BasicBlock::insertInto (BasicBlock.cpp) asserts
   "!Parent && "Already has a parent"" and the list traits assert
   "!V->getParent() && "Value already in a container!!""
   (SymbolTableListTraitsImpl.h) for an attached block, so the wrapper's
   parent check must reject this: this is a Fil-C panic (exit 42) instead of
   that assertion abort.
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
    LLVMAppendExistingBasicBlock(fn_b, entry_a);

    return 0;
}
