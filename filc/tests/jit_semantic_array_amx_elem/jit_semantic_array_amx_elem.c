#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMArrayType(X86_AMX, 4): ArrayType::isValidElementType (llvm/lib/IR/
   Type.cpp) excludes X86_AMX along with void, label, metadata, function, and
   token, and ArrayType::get asserts "Invalid type for array element!".
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef amx = LLVMX86AMXTypeInContext(context);
    LLVMArrayType(amx, 4);
    return 0;
}
