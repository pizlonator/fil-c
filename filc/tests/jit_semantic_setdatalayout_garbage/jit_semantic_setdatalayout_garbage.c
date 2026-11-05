#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMSetDataLayout(module, "garbage!!") has an unknown specifier.
   Module::setDataLayout aborts via the DataLayout(StringRef) constructor,
   which reports the same Error that the graceful DataLayout::parse returns
   (DataLayout.h), so the shim's ShimDataLayoutSpecOK predicate, which parses
   instead of constructing, must reject this: this is a Fil-C panic (exit 42)
   instead of that "LLVM ERROR: unknown specifier" abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMSetDataLayout(module, "garbage!!");
    return 0;
}
