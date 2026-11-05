#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMConstIntOfString with a sign but no digits.  APInt::fromString
   (APInt.cpp) asserts "String is only a sign, needs a value." for that, so
   the shim's ShimAPIntStringOK predicate, which mirrors fromString's
   conditions, must reject this: this is a Fil-C panic (exit 42) instead of
   that assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMConstIntOfString(i32, "-", 10);
    return 0;
}
