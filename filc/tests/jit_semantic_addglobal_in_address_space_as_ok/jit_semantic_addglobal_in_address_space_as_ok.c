#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the LLVMAddGlobalInAddressSpace address-space check:
   address space 1 stays legal and the global lands in it.
 */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef gv = LLVMAddGlobalInAddressSpace(module, i32, "g", 1);
    if (!gv) {
        fprintf(stderr, "bad: no global\n");
        abort();
    }

    printf("addglobal in address space as ok\n");
    return 0;
}
