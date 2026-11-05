#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstRealOfString and LLVMConstRealOfStringAndSize with valid
   floating-point literals are legal, in decimal and C99 hexadecimal forms. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef flt = LLVMFloatTypeInContext(context);
    LLVMValueRef a = LLVMConstRealOfString(flt, "1.5");
    LLVMValueRef b = LLVMConstRealOfStringAndSize(flt, "0x1.8p0", 7);
    double da;
    double db;
    LLVMBool la;
    LLVMBool lb;
    la = 0;
    lb = 0;
    da = LLVMConstRealGetDouble(a, &la);
    db = LLVMConstRealGetDouble(b, &lb);
    if (la || lb || da != 1.5 || db != 1.5) {
        fprintf(stderr, "bad: wrong values\n");
        abort();
    }
    (void)module;

    printf("constrealofstring ok\n");
    return 0;
}
