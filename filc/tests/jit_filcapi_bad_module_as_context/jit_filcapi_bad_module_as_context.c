#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* ...TypeInContext constructors want an LLVM context; passing an LLVM module
   must panic (exit 42) instead of reading the module payload as a context. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMInt32TypeInContext((LLVMContextRef)module);
    return 0;
}
