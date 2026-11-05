#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMInsertExistingBasicBlockAfterInsertBlock with a builder that has no
   insert position.  Core.cpp asserts "current insertion point is invalid!"
   before it can even look at the block, so the wrapper's insert-position
   check must reject this: this is a Fil-C panic (exit 42) instead of that
   assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMBasicBlockRef orphan = LLVMCreateBasicBlockInContext(context, "orphan");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMInsertExistingBasicBlockAfterInsertBlock(builder, orphan);

    return 0;
}
