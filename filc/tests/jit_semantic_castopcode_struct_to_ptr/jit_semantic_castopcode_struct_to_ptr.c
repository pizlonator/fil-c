#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMGetCastOpcode of a struct value to a pointer type.
   CastInst::getCastOpcode (Instructions.cpp) ends in llvm_unreachable
   "Casting pointer to other than pointer or int" for the combination, so the
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
    LLVMTypeRef ptr = LLVMPointerTypeInContext(context, 0);
    LLVMValueRef cs = LLVMGetUndef(st);
    LLVMGetCastOpcode(cs, false, ptr, false);

    return 0;
}
