#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMAddGlobal(opaque struct) is legal (LLVM-lenient). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef opaque = LLVMStructCreateNamed(context, "opq");
    LLVMValueRef g = LLVMAddGlobal(module, opaque, "g");
    if (!g) {
        fprintf(stderr, "bad: no global\n");
        abort();
    }

    printf("addglobal opaque ok\n");
    return 0;
}
