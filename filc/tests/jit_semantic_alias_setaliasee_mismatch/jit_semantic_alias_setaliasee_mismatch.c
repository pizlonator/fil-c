#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMAliasSetAliasee with a ConstantInt aliasee on a function alias.
   GlobalAlias::setAliasee (Globals.cpp) asserts "Alias and aliasee types
   should match!" when the aliasee's type differs from the alias's own type,
   so the wrapper's aliasee-type check must reject this: this is a Fil-C
   panic (exit 42) instead of that assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fn_ty = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fn_ty);
    LLVMValueRef alias = LLVMAddAlias2(module, fn_ty, 0, fn, "alias");
    LLVMValueRef ci32 = LLVMConstInt(i32, 42, false);
    LLVMAliasSetAliasee(alias, ci32);

    return 0;
}
