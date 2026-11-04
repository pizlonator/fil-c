#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

int main()
{
    FilLLVMJIT_GetPointerToGlobal(NULL, "x");
    return 0;
}
