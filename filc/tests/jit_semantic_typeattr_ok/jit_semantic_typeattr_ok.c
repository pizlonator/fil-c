#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the LLVMCreateTypeAttribute type-attribute-kind
   check: a real type-attribute kind (sret, fetched by name) with a pointer
   type stays legal.
 */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    unsigned kind = LLVMGetEnumAttributeKindForName("sret", 4);
    LLVMTypeRef ptr = LLVMPointerTypeInContext(context, 0);
    LLVMAttributeRef attr = LLVMCreateTypeAttribute(context, kind, ptr);
    if (!attr) {
        fprintf(stderr, "bad: no attribute\n");
        abort();
    }

    printf("type attr ok\n");
    return 0;
}
