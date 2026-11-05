#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMConstAdd over an i32 and an i64 constant.  The wrapper's operand-type
   check rejects the mismatch, so this is a Fil-C panic (exit 42) instead of
   the "Operand types in binary constant expression should match" assertion
   abort in Constants.cpp.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64 = LLVMInt64TypeInContext(context);

    LLVMValueRef a = LLVMConstInt(i32, 1, false);
    LLVMValueRef b = LLVMConstInt(i64, 2, false);
    LLVMConstAdd(a, b);

    return 0;
}
