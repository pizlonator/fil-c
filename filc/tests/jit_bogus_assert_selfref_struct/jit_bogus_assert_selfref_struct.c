#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMStructSetBody on an identified struct whose body contains the struct
   itself by value.  StructType::setBody wraps setBodyOrError in cantFail, so
   the "identified structure type 'selfref' is recursive" error aborts
   (SIGABRT) at the setBody call. */

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
