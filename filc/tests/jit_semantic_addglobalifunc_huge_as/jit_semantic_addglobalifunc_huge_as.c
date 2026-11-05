#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMAddGlobalIFunc with an address space of 2^24.  GlobalIFunc::create
   calls PointerType::get, whose 24-bit SubclassData field trips
   Type::setSubclassData's "Subclass data too large for field" assertion
   (Type.h), so the wrapper's address-space check must reject it: this is a
   Fil-C panic (exit 42) instead of that assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fnty = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef resolver = LLVMConstInt(i32, 0, false);
    LLVMAddGlobalIFunc(module, "if", 2, fnty, 0x1000000, resolver);

    return 0;
}
