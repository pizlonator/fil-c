#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstNull on the token type is legal: Constant::getNullValue returns
   TokenNone for it. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMValueRef null_tok = LLVMConstNull(LLVMTokenTypeInContext(context));
    if (!null_tok) {
        fprintf(stderr, "bad: no token none\n");
        abort();
    }
    printf("token null ok\n");
    return 0;
}
