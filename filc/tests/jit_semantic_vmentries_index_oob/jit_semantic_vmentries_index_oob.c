#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMValueMetadataEntriesGetMetadata with an index past the entry count.
   The getter indexes the raw array, whose length LLVM does not expose, so an
   out-of-range read returns a wild metadata reference; the entries object
   records its length so the wrapper can panic instead. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef global = LLVMAddGlobal(module, i32, "g");
    LLVMGlobalSetMetadata(global, 7, LLVMMDNodeInContext2(context, NULL, 0));
    size_t len = 0;
    LLVMValueMetadataEntry* entries = LLVMGlobalCopyAllMetadata(global, &len);
    if (len != 1) {
        fprintf(stderr, "bad: expected one metadata entry\n");
        abort();
    }
    LLVMValueMetadataEntriesGetMetadata(entries, 9);
    return 0;
}
