#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMCreateTypeAttribute with an attribute kind that is not a type kind
   (readonly).  Attribute::get(Context, Kind, Type) (Attributes.cpp) asserts
   "Not a type attribute", so the wrapper's type-attribute-kind check (the
   shim's ShimKindIsTypeAttr predicate) must reject it: this is a Fil-C panic
   (exit 42) instead of that assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    unsigned kind = LLVMGetEnumAttributeKindForName("readonly", 8);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMCreateTypeAttribute(context, kind, i32);

    return 0;
}
