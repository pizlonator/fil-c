#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMAddAttributeAtIndex(fn, 0xFFFFFFFE, attr) on a function with no
   parameters.  AttributeList::setAttributesAtIndex (llvm/lib/IR/
   Attributes.cpp) maps the index with attrIdxToArrayIdx(Index) = Index + 1
   and then does AttrSets.resize(Index + 1); AttrSets[Index] in unsigned
   arithmetic, so the resize wraps to zero and the subscript writes 32GB past
   the SmallVector (probed: SmallVector::operator[]'s "idx < size()" assert).
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef fnTy = LLVMFunctionType(LLVMInt32TypeInContext(context),
                                        NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fnTy);
    LLVMAddAttributeAtIndex(fn, 0xFFFFFFFEu,
                            LLVMCreateEnumAttribute(context, 1, 0));
    return 0;
}
