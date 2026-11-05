#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMConstRealOfString(flt, "garbage") is not a floating-point literal.
   ConstantFP::get (Constants.cpp) builds APFloat(Semantics, Str), whose
   constructor asserts "Invalid floating point representation" on the Error
   that convertFromString returns, so the shim's ShimAPFloatStringOK
   predicate, which runs that graceful conversion, must reject this: this is
   a Fil-C panic (exit 42) instead of that assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef flt = LLVMFloatTypeInContext(context);
    LLVMConstRealOfString(flt, "garbage");
    return 0;
}
