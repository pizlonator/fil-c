/* LLVM C API functions that take no context, module or value must work
   without one: they used to lock a NULL context and crash. */
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
    printf("ok\n");
    return 0;
}
