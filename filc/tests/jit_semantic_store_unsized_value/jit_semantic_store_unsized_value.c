#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildStore of an unsized value (void undef).  StoreInst's constructor
   sizes the value's type through DataLayout, which asserts Ty->isSized()
   ("Cannot getTypeInfo() on a type that is unsized!", DataLayout.cpp); the
   load wrappers make the same demand on their type operand.  This must panic
   in the wrapper instead of tripping that assertion.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef ptr_ty = LLVMPointerTypeInContext(context, 0);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef slot = LLVMBuildAlloca(builder, ptr_ty, "slot");
    LLVMBuildStore(builder, LLVMGetUndef(LLVMVoidTypeInContext(context)), slot);
    return 0;
}
