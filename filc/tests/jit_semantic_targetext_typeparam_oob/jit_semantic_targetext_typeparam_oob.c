#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMGetTargetExtTypeTypeParam with an index past the type's parameter
   count.  The accessor goes through Type::getContainedType, which asserts
   "Index out of range!" (Type.h).  This must panic in the wrapper instead of
   tripping that assertion.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef params[1] = { LLVMInt32TypeInContext(context) };
    LLVMTypeRef ty = LLVMTargetExtTypeInContext(context, "test.type",
                                                params, 1, NULL, 0);
    if (LLVMGetTargetExtTypeNumTypeParams(ty) != 1) {
        fprintf(stderr, "bad: type param count\n");
        abort();
    }
    LLVMGetTargetExtTypeTypeParam(ty, 5);
    return 0;
}
