#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMAddGlobalIFunc with an embedded NUL inside the length-delimited name
   ("i\0f", length 3).  Core.cpp hands StringRef(Name, NameLen) to
   GlobalIFunc::create, whose GlobalValue constructor calls setName, and
   Value::setNameImpl (Value.cpp) asserts "Null bytes are not allowed in
   names", so the wrapper's name check must reject this: this is a Fil-C
   panic (exit 42) instead of that assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fnty = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef resolver = LLVMConstInt(i32, 0, false);
    LLVMAddGlobalIFunc(module, "i\0f", 3, fnty, 0, resolver);

    return 0;
}
