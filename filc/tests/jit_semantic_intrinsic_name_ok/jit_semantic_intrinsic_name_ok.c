#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the intrinsic-id checks on the two hand-written name
   natives: LLVMIntrinsicGetName of a non-overloaded intrinsic and
   LLVMIntrinsicCopyOverloadedName2 of an overloaded one stay legal and
   return their names.
 */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    unsigned trap_id = LLVMLookupIntrinsicID("llvm.trap", 9);
    unsigned abs_id = LLVMLookupIntrinsicID("llvm.abs", 8);
    size_t length = 0;
    const char* name;
    char* name2;
    if (trap_id == 0 || abs_id == 0) {
        fprintf(stderr, "bad: intrinsic lookup failed\n");
        abort();
    }
    name = LLVMIntrinsicGetName(trap_id, &length);
    if (!name || name[0] != 'l') {
        fprintf(stderr, "bad: no intrinsic name\n");
        abort();
    }
    LLVMTypeRef i64 = LLVMInt64TypeInContext(context);
    LLVMTypeRef params[1] = { i64 };
    length = 0;
    name2 = LLVMIntrinsicCopyOverloadedName2(module, abs_id, params, 1,
                                             &length);
    if (!name2 || name2[0] != 'l') {
        fprintf(stderr, "bad: no overloaded intrinsic name\n");
        abort();
    }
    free(name2);

    printf("intrinsic name ok\n");
    return 0;
}
