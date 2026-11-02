#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef context = LLVMContextCreate();

    /* A constant-range attribute is creatable through the exposed
       LLVMCreateConstantRangeAttribute, but it is neither an enum nor an
       integer attribute.  This must panic in the wrapper instead of tripping
       Attribute::getValueAsInt's assertion. */
    unsigned kindID = LLVMGetEnumAttributeKindForName("range", 5);
    unsigned long long lowerWords[1] = { 0 };
    unsigned long long upperWords[1] = { 10 };
    LLVMAttributeRef rangeAttr = LLVMCreateConstantRangeAttribute(
        context, kindID, 32, lowerWords, upperWords);
    unsigned long long value = LLVMGetEnumAttributeValue(rangeAttr);

    (void)value;
    return 0;
}
