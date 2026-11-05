#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMCreateEnumAttribute with a nonzero value on a pure enum kind.
   Attribute::get (Attributes.cpp) asserts "Value must be zero for enum
   attributes", so the wrapper's attribute check must reject this: this is a
   Fil-C panic (exit 42) instead of that assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    unsigned nocapture = LLVMGetEnumAttributeKindForName("nocapture", 9);
    LLVMCreateEnumAttribute(context, nocapture, 5);

    return 0;
}
