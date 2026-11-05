#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMConstXor over two float constants.  The wrapper's operand-type check
   rejects non-integer operands, so this is a Fil-C panic (exit 42) instead of
   the logical-operation assertion abort in Constants.cpp.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef flt = LLVMFloatTypeInContext(context);

    LLVMValueRef cf = LLVMConstReal(flt, 1.5);
    LLVMConstXor(cf, cf);

    return 0;
}
