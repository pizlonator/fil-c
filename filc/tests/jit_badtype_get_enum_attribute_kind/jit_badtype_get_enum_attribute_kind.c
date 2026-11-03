#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMAttributeRef strAttr = LLVMCreateStringAttribute(ctx, "k", 1, "v", 1);

    /* LLVMGetEnumAttributeKind requires an enum, integer, or type attribute;
       the string attribute strAttr is not one.  This must panic in the wrapper
       instead of tripping an LLVM assertion. */
    LLVMGetEnumAttributeKind(strAttr);

    return 0;
}
