#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef ctx = LLVMContextCreate();
    unsigned kindID = LLVMGetEnumAttributeKindForName("alwaysinline", 12);
    LLVMAttributeRef enumAttr = LLVMCreateEnumAttribute(ctx, kindID, 0);

    /* LLVMGetTypeAttributeValue requires a type attribute; the enum attribute
       enumAttr is not one.  This must panic in the wrapper instead of tripping
       an LLVM assertion. */
    LLVMGetTypeAttributeValue(enumAttr);

    return 0;
}
