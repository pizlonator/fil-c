#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMGetCastOpcode of an integer value to void.  CastInst::getCastOpcode
   (Instructions.cpp) asserts "Only first class types are castable!" (void is
   not first-class), so the wrapper's cast-opcode check must reject it: this
   is a Fil-C panic (exit 42) instead of that assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef ci = LLVMConstInt(i32, 5, false);
    LLVMGetCastOpcode(ci, false, LLVMVoidTypeInContext(context), false);

    return 0;
}
