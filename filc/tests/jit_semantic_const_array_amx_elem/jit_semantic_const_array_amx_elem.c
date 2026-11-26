#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstArray(X86_AMX, { undef X86_AMX }, 1): LLVMConstArray calls
   ArrayType::get with the element type before touching the initializers
   (llvm/lib/IR/Core.cpp), and ArrayType::isValidElementType (Type.cpp)
   excludes X86_AMX, so the "Invalid type for array element!" assert fires.
   The X86_AMX undef keeps sem_const_array's initializer-type check quiet so
   that this pins the element-type check.  Deliberate tripwire: the
   manifest's output-excludes greps for LLVM's assert text. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef amx = LLVMX86AMXTypeInContext(context);
    LLVMValueRef vals[1] = { LLVMGetUndef(amx) };
    LLVMConstArray(amx, vals, 1);
    return 0;
}
