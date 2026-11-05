#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMSetValueName2 with an embedded NUL inside the length ("a\0b", length
   3).  Value::setNameImpl (Value.cpp) asserts "Null bytes are not allowed in
   names", so the wrapper's name check must reject this: this is a Fil-C
   panic (exit 42) instead of that assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef global = LLVMAddGlobal(module, i32, "g");
    LLVMSetValueName2(global, "a\0b", 3);

    return 0;
}
