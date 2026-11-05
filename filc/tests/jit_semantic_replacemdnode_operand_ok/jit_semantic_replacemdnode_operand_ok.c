#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the LLVMReplaceMDNodeOperandWith index check: an
   in-range operand replacement on a one-operand node stays legal. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMValueRef operand = LLVMMDStringInContext(context, "a", 1);
    LLVMValueRef mdnode = LLVMMDNodeInContext(context, &operand, 1);
    if (LLVMGetMDNodeNumOperands(mdnode) != 1) {
        fprintf(stderr, "bad: node does not have one operand\n");
        abort();
    }
    LLVMReplaceMDNodeOperandWith(mdnode, 0,
        LLVMMDStringInContext2(context, "b", 1));
    printf("replacemdnode ok\n");
    return 0;
}
