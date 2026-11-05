#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMFunctionType with a function-typed parameter.  FunctionType's
   constructor asserts isValidArgumentType (Type.cpp: "Not a valid type for
   function argument!"), which in this LLVM is everything except function
   and void, so a function type in the parameter list must panic in the
   wrapper instead of tripping that assertion.  Deliberate tripwire: the
   manifest's output-excludes greps for LLVM's assert text, so if the check
   ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fn_ty = LLVMFunctionType(i32, NULL, 0, false);
    LLVMTypeRef params[1];
    params[0] = fn_ty;
    LLVMFunctionType(i32, params, 1, false);
    return 0;
}
