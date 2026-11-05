#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMGetCastOpcode of an integer value to a vector of a different size.
   CastInst::getCastOpcode (Instructions.cpp) asserts "Illegal cast to vector
   (wrong type or size)", so the wrapper's cast-opcode check must reject it:
   this is a Fil-C panic (exit 42) instead of that assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef v = LLVMVectorType(i32, 4);
    LLVMValueRef ci = LLVMConstInt(i32, 5, false);
    LLVMGetCastOpcode(ci, false, v, false);

    return 0;
}
