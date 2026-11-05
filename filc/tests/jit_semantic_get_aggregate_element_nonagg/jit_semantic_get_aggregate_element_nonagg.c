#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMGetAggregateElement on a constant whose type is neither an aggregate
   nor a vector.  Constant::getAggregateElement asserts "Must be an
   aggregate/vector constant" (llvm/lib/IR/Constants.cpp) for it.  The row's
   subclass check only pins that the input is a llvm::Constant, so the type
   check is a separate clause; see jit_semantic_get_aggregate_element_ok for
   the legal shapes. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef c = LLVMConstInt(i32, 0, false);

    LLVMGetAggregateElement(c, 0);

    return 0;
}
