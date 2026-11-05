#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstIntOfString and LLVMConstIntOfStringAndSize with valid digit
   strings for their radixes are legal, including a signed value and radix 36
   letters. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef a = LLVMConstIntOfString(i32, "12345", 10);
    LLVMValueRef b = LLVMConstIntOfStringAndSize(i32, "-7", 2, 10);
    LLVMValueRef c = LLVMConstIntOfString(i32, "ff", 16);
    LLVMValueRef d = LLVMConstIntOfString(i32, "z", 36);
    if (LLVMConstIntGetZExtValue(a) != 12345 ||
        LLVMConstIntGetSExtValue(b) != -7 ||
        LLVMConstIntGetZExtValue(c) != 255 ||
        LLVMConstIntGetZExtValue(d) != 35) {
        fprintf(stderr, "bad: wrong values\n");
        abort();
    }
    (void)module;

    printf("constintofstring ok\n");
    return 0;
}
