#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMGetCastOpcode of a floating-point source value to a pointer type.
   CastInst::getCastOpcode (Instructions.cpp) ends in llvm_unreachable
   "Casting pointer or non-first class to float" for the combination, so the
   wrapper's cast-opcode check must reject it: this is a Fil-C panic (exit 42)
   instead of that unreachable.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef flt = LLVMFloatTypeInContext(context);
    LLVMTypeRef ptr = LLVMPointerTypeInContext(context, 0);
    LLVMValueRef cf = LLVMConstReal(flt, 1.5);
    LLVMGetCastOpcode(cf, false, ptr, false);

    return 0;
}
