#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMStructSetBody on an identified struct whose body contains the struct
   itself by value.  The shim's ShimStructSetBodyOK predicate (the same walk
   that StructType::checkBody does) rejects the recursive body, so this is a
   Fil-C panic (exit 42) instead of the cantFail abort on "identified
   structure type 'selfref' is recursive".
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);

    LLVMTypeRef selfref = LLVMStructCreateNamed(context, "selfref");
    LLVMTypeRef elements[1] = { selfref };
    LLVMStructSetBody(selfref, elements, 1, false);

    LLVMAddGlobal(module, selfref, "g");

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_CompileModule(jit, module);

    return 0;
}
