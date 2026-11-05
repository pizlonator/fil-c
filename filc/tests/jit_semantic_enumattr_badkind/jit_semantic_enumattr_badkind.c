#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMCreateEnumAttribute with a kind id that is no attribute at all.
   Attribute::get (Attributes.cpp) asserts "Not an enum or int attribute" for
   such a kind, so the wrapper's attribute check must reject this: this is a
   Fil-C panic (exit 42) instead of that assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMCreateEnumAttribute(context, 1000000, 0);

    return 0;
}
