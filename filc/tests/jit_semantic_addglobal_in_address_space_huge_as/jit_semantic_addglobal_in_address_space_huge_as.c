#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMAddGlobalInAddressSpace with an address space of 2^24.  The global
   variable constructor calls PointerType::get, whose 24-bit SubclassData
   field trips Type::setSubclassData's "Subclass data too large for field"
   assertion (Type.h), so the wrapper's address-space check must reject it:
   this is a Fil-C panic (exit 42) instead of that assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMAddGlobalInAddressSpace(module, i32, "g", 0x1000000);

    return 0;
}
