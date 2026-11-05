#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the metadata-entries index checks: in-range indices
   return the entry's kind and metadata. */

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
    if (LLVMValueMetadataEntriesGetKind(entries, 0) != 7 ||
        !LLVMValueMetadataEntriesGetMetadata(entries, 0)) {
        fprintf(stderr, "bad: entry did not come back\n");
        abort();
    }
    printf("vmentries ok\n");
    return 0;
}
