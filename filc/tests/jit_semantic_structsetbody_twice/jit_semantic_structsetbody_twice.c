#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMStructSetBody on a named struct whose body was already set.
   StructType::setBodyOrError (Type.cpp) asserts "isOpaque() && "Struct body
   already set!"" for an already-defined struct, so the wrapper's body check
   must reject this: this is a Fil-C panic (exit 42) instead of that
   assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef named = LLVMStructCreateNamed(context, "named");
    LLVMTypeRef elements[1] = { i32 };
    LLVMStructSetBody(named, elements, 1, false);
    LLVMStructSetBody(named, elements, 1, false);

    return 0;
}
