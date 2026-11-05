#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the address-space checks on the pointer-type
   creators: address spaces 0, 1, and 2^24-1 (the largest value the 24-bit
   SubclassData field holds) all stay legal, through both
   LLVMPointerTypeInContext and LLVMPointerType.
 */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef t0 = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef t1 = LLVMPointerTypeInContext(context, 1);
    LLVMTypeRef tm = LLVMPointerTypeInContext(context, 0xffffff);
    LLVMTypeRef s0 = LLVMPointerType(i32, 0);
    if (LLVMGetPointerAddressSpace(t0) != 0 ||
        LLVMGetPointerAddressSpace(t1) != 1 ||
        LLVMGetPointerAddressSpace(tm) != 0xffffff ||
        LLVMGetPointerAddressSpace(s0) != 0) {
        fprintf(stderr, "bad: wrong address space\n");
        abort();
    }

    printf("pointertype as ok\n");
    return 0;
}
