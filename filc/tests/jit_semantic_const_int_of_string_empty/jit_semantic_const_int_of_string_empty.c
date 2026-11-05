#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMConstIntOfStringAndSize with a null string of length zero.
   APInt::fromString (APInt.cpp) asserts "Invalid string length" for an empty
   string, so the shim's ShimAPIntStringOK predicate, which mirrors
   fromString's conditions, must reject this: this is a Fil-C panic (exit 42)
   instead of that assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMConstIntOfStringAndSize(i32, NULL, 0, 10);
    return 0;
}
