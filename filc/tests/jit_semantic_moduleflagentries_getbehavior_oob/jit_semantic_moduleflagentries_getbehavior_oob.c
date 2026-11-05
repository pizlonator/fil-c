#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* LLVMModuleFlagEntriesGetFlagBehavior with an index past the entry count.  The
   entries array is indexed raw, and the object records its length (see
   LLVMCopyModuleFlagsMetadata), so an out-of-range index reads past
   LLVM-owned memory; the wrapper must panic instead. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMMetadataRef md = LLVMValueAsMetadata(LLVMConstInt(i32, 7, false));
    LLVMAddModuleFlag(module, LLVMModuleFlagBehaviorError, "aaa", 3, md);
    LLVMAddModuleFlag(module, LLVMModuleFlagBehaviorOverride, "bbb", 3, md);
    size_t count = 0;
    LLVMModuleFlagEntry* entries = LLVMCopyModuleFlagsMetadata(module, &count);
    if (count != 2 || !entries) {
        fprintf(stderr, "bad: expected two module flags\n");
        abort();
    }
    LLVMModuleFlagEntriesGetFlagBehavior(entries, 7);
    return 0;
}
