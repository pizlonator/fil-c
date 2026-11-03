#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Compiling a completely empty module is fine: no functions, no globals.
   GetPointerToGlobal on an unknown name must return NULL (no panic). */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    if (!result) {
        fprintf(stderr, "bad: expected an executable module\n");
        abort();
    }

    void* anything = FilLLVMJIT_GetPointerToGlobal(result, "anything");
    if (anything) {
        fprintf(stderr, "bad: expected NULL for unknown symbol\n");
        abort();
    }

    printf("empty module ok\n");
    return 0;
}
