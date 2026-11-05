#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMBuildOr over two loaded pointers.  The wrapper's semantic check
   rejects non-integer operand types, so this is a Fil-C panic (exit 42)
   instead of BinaryOperator::AssertOK's assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef ptrT = LLVMPointerTypeInContext(context, 0);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef slot = LLVMBuildAlloca(builder, ptrT, "slot");
    LLVMValueRef lw = LLVMBuildLoad2(builder, ptrT, slot, "v");
    LLVMBuildOr(builder, lw, lw, "or");

    return 0;
}
