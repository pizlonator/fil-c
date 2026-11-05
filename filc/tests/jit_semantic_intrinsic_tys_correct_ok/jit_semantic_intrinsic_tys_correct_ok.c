#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Success controls for the intrinsic type-table decode check: real
   overloaded intrinsics with their correct parameter types must still
   decode, declare, and mangle.  The type parameters are derived from the
   intrinsic definitions: llvm.fma takes one (the float), llvm.masked.load
   takes two (the loaded vector and the pointer's element type), llvm.memset
   takes two (the pointer and the length's integer), llvm.ctlz takes one
   (the integer), and llvm.annotation takes two (the annotated integer and
   the pointer's element type). */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64 = LLVMInt64TypeInContext(context);
    LLVMTypeRef f32 = LLVMFloatTypeInContext(context);
    LLVMTypeRef ptr = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef v4f32 = LLVMVectorType(f32, 4);
    unsigned fma_id = LLVMLookupIntrinsicID("llvm.fma", 8);
    unsigned masked_load_id = LLVMLookupIntrinsicID("llvm.masked.load", 16);
    unsigned memset_id = LLVMLookupIntrinsicID("llvm.memset", 11);
    unsigned ctlz_id = LLVMLookupIntrinsicID("llvm.ctlz", 9);
    unsigned annotation_id = LLVMLookupIntrinsicID("llvm.annotation", 15);
    if (!fma_id || !masked_load_id || !memset_id || !ctlz_id ||
        !annotation_id) {
        fprintf(stderr, "bad: intrinsic lookup failed\n");
        abort();
    }
    LLVMTypeRef fma_tys[1];
    fma_tys[0] = f32;
    if (LLVMGetTypeKind(LLVMIntrinsicGetType(context, fma_id, fma_tys, 1)) !=
        LLVMFunctionTypeKind) {
        fprintf(stderr, "bad: fma type\n");
        abort();
    }
    LLVMValueRef fma = LLVMGetIntrinsicDeclaration(module, fma_id, fma_tys, 1);
    if (!fma || LLVMGetIntrinsicID(fma) != fma_id) {
        fprintf(stderr, "bad: fma declaration\n");
        abort();
    }
    LLVMTypeRef masked_load_tys[2];
    masked_load_tys[0] = v4f32;
    masked_load_tys[1] = ptr;
    if (LLVMGetTypeKind(LLVMIntrinsicGetType(context, masked_load_id,
                                             masked_load_tys, 2)) !=
        LLVMFunctionTypeKind) {
        fprintf(stderr, "bad: masked.load type\n");
        abort();
    }
    LLVMTypeRef memset_tys[2];
    memset_tys[0] = ptr;
    memset_tys[1] = i32;
    if (LLVMGetTypeKind(LLVMIntrinsicGetType(context, memset_id, memset_tys,
                                             2)) != LLVMFunctionTypeKind) {
        fprintf(stderr, "bad: memset type\n");
        abort();
    }
    LLVMTypeRef ctlz_tys[1];
    ctlz_tys[0] = i64;
    LLVMValueRef ctlz = LLVMGetIntrinsicDeclaration(module, ctlz_id, ctlz_tys, 1);
    if (!ctlz || LLVMGetIntrinsicID(ctlz) != ctlz_id) {
        fprintf(stderr, "bad: ctlz declaration\n");
        abort();
    }
    /* llvm.annotation with (f32, f32) mangles to llvm.annotation.f32.f32. */
    LLVMTypeRef annotation_tys[2];
    annotation_tys[0] = f32;
    annotation_tys[1] = f32;
    LLVMValueRef annotation =
        LLVMGetIntrinsicDeclaration(module, annotation_id, annotation_tys, 2);
    if (!annotation || LLVMGetIntrinsicID(annotation) != annotation_id) {
        fprintf(stderr, "bad: annotation declaration\n");
        abort();
    }
    size_t name_len = 0;
    char* name = LLVMIntrinsicCopyOverloadedName2(module, annotation_id,
                                                  annotation_tys, 2,
                                                  &name_len);
    if (!name || name_len != strlen("llvm.annotation.f32.f32") ||
        strncmp(name, "llvm.annotation.f32.f32", name_len)) {
        fprintf(stderr, "bad: annotation name (%.*s)\n", (int)name_len,
                name ? name : "");
        abort();
    }
    free(name);
    printf("intrinsic tys correct ok\n");
    return 0;
}
