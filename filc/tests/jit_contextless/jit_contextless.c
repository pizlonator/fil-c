/* LLVM C API functions that take no context, module or value must work
   without one: they used to lock a NULL context and crash.  This covers all
   six context-free wrappers: LLVMLookupIntrinsicID, LLVMIntrinsicIsOverloaded,
   LLVMGetUndefMaskElem, LLVMIsMultithreaded, LLVMGetEnumAttributeKindForName,
   and LLVMGetLastEnumAttributeKind. */
#include <filc_llvm_jit.h>
#include <stdfil.h>
#include <stdio.h>
#include <string.h>

int main()
{
    const char* name = "llvm.memcpy";
    unsigned id = LLVMLookupIntrinsicID(name, strlen(name));
    ZASSERT(id);
    ZASSERT(LLVMIntrinsicIsOverloaded(id));
    ZASSERT(!LLVMLookupIntrinsicID("llvm.not.an.intrinsic", 21));
    ZASSERT(LLVMGetUndefMaskElem() == -1);
    LLVMIsMultithreaded();
    unsigned kind = LLVMGetEnumAttributeKindForName("memory", 6);
    ZASSERT(kind);
    ZASSERT(kind <= LLVMGetLastEnumAttributeKind());
    ZASSERT(!LLVMGetEnumAttributeKindForName("no.such.attribute", 17));
    printf("ok\n");
    return 0;
}
