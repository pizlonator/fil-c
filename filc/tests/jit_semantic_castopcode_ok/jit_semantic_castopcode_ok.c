#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the LLVMGetCastOpcode cast-opcode check: every shape
   CastInst::getCastOpcode (Instructions.cpp) actually picks an opcode for
   stays legal.  Covers int-to-smaller-int (trunc), int-to-float (sitofp),
   float-to-int (fptosi), float widen/narrow (fpext/fptrunc), pointer-to-int
   (ptrtoint), int-to-pointer (inttoptr), a same-size vector bitcast, and a
   same-type no-op.
 */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef i8 = LLVMInt8TypeInContext(context);
    LLVMTypeRef i64 = LLVMInt64TypeInContext(context);
    LLVMTypeRef flt = LLVMFloatTypeInContext(context);
    LLVMTypeRef dbl = LLVMDoubleTypeInContext(context);
    LLVMTypeRef ptr = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef v4i = LLVMVectorType(i32, 4);
    LLVMTypeRef v4f = LLVMVectorType(flt, 4);
    LLVMValueRef ci = LLVMConstInt(i32, 5, false);
    LLVMValueRef cf = LLVMConstReal(flt, 1.5);
    LLVMValueRef cd = LLVMConstReal(dbl, 1.5);
    LLVMValueRef cp = LLVMConstNull(ptr);
    LLVMValueRef cv = LLVMGetUndef(v4i);

    if (LLVMGetCastOpcode(ci, false, i8, false) != LLVMTrunc ||
        LLVMGetCastOpcode(ci, true, flt, false) != LLVMSIToFP ||
        LLVMGetCastOpcode(cf, false, i32, false) != LLVMFPToUI ||
        LLVMGetCastOpcode(cd, false, flt, false) != LLVMFPTrunc ||
        LLVMGetCastOpcode(cf, false, dbl, false) != LLVMFPExt ||
        LLVMGetCastOpcode(cp, false, i64, false) != LLVMPtrToInt ||
        LLVMGetCastOpcode(ci, false, ptr, false) != LLVMIntToPtr ||
        LLVMGetCastOpcode(cv, false, v4f, false) != LLVMUIToFP ||
        LLVMGetCastOpcode(ci, false, i32, false) != LLVMBitCast) {
        fprintf(stderr, "bad: unexpected cast opcode\n");
        abort();
    }

    printf("castopcode ok\n");
    return 0;
}
