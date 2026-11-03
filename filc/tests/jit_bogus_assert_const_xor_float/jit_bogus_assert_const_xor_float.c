#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMConstXor over two float constants: the logical-operation assertion in
   Constants.cpp aborts (SIGABRT). */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef flt = LLVMFloatTypeInContext(context);

    LLVMValueRef cf = LLVMConstReal(flt, 1.5);
    LLVMConstXor(cf, cf);

    return 0;
}
