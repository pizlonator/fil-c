#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMAddAlias2 with a ConstantInt aliasee.  The alias gets
   PointerType::get(Context, AddrSpace) (GlobalValue's constructor ignores
   the pointee), and GlobalAlias::setAliasee (Globals.cpp) then asserts
   "Alias and aliasee types should match!" against the aliasee's own type,
   which for a ConstantInt is not a pointer at all, so the wrapper's
   aliasee-type check must reject this: this is a Fil-C panic (exit 42)
   instead of that assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef ci32 = LLVMConstInt(i32, 42, false);
    LLVMAddAlias2(module, i32, 0, ci32, "bad");

    return 0;
}
