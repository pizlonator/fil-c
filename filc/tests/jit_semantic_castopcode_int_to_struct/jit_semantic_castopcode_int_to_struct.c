#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMGetCastOpcode of an integer value to a struct type.
   CastInst::getCastOpcode (Instructions.cpp) ends in llvm_unreachable
   "Casting to type that is not first-class" for the combination, so the
   wrapper's cast-opcode check must reject it: this is a Fil-C panic (exit 42)
   instead of that unreachable.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef st = LLVMStructCreateNamed(context, "s");
    LLVMStructSetBody(st, &i32, 1, false);
    LLVMValueRef ci = LLVMConstInt(i32, 1, false);
    LLVMGetCastOpcode(ci, false, st, false);

    return 0;
}
