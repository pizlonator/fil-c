#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstNull on a target extension type without the zeroinit property.
   ConstantTargetNone::get asserts Ty->hasProperty(HasZeroInit) ("Target
   extension type not allowed to have a zeroinitializer", Constants.cpp);
   the wrapper's type check must reject it first. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMConstNull(LLVMTargetExtTypeInContext(context, "test.foo", NULL, 0,
                                             NULL, 0));
    return 0;
}
