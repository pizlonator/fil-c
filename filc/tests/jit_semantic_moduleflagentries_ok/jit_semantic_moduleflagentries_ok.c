#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Success control for the module-flag-entries getters: a module with two
   flags, copied through LLVMCopyModuleFlagsMetadata, returns each entry's
   key string, flag behavior, and metadata through the in-range getters. */

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
    static const char* keys[2] = { "aaa", "bbb" };
    static const unsigned behaviors[2] = {
        LLVMModuleFlagBehaviorError,
        LLVMModuleFlagBehaviorOverride
    };
    unsigned i;
    for (i = 0; i < count; ++i) {
        size_t len = 0;
        const char* key = LLVMModuleFlagEntriesGetKey(entries, i, &len);
        if (!key || strncmp(key, keys[i], 3)) {
            fprintf(stderr, "bad: flag %u key (%s)\n", i, key ? key : "");
            abort();
        }
        if (LLVMModuleFlagEntriesGetFlagBehavior(entries, i) !=
            behaviors[i]) {
            fprintf(stderr, "bad: flag %u behavior\n", i);
            abort();
        }
        if (!LLVMModuleFlagEntriesGetMetadata(entries, i)) {
            fprintf(stderr, "bad: flag %u metadata\n", i);
            abort();
        }
    }
    printf("module flag entries ok\n");
    return 0;
}
