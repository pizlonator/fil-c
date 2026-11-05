#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMReplaceMDNodeOperandWith with an index past the node's operand count.
   MDNode::replaceOperandWith reads the old operand through getOperand, which
   asserts I < getNumOperands ("Out of range", Metadata.h).  This must panic
   in the wrapper instead of tripping that assertion.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMValueRef mdnode = LLVMMDNodeInContext(context, NULL, 0);
    LLVMReplaceMDNodeOperandWith(mdnode, 9,
        LLVMMDStringInContext2(context, "y", 1));
    return 0;
}
