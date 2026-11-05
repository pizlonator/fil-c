#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstIntOfArbitraryPrecision with a zero word count and a non-null
   words pointer.  APInt(ArrayRef<uint64_t>) and APInt::initFromArray index
   the words unconditionally, so the empty array dies in ArrayRef::operator[]
   ("Invalid index!", ArrayRef.h) even though the pointer is valid, and the
   old check's predicate (num_words != 0 || words != NULL) let this shape
   through.  The fixed check rejects a zero word count outright: this is a
   Fil-C panic (exit 42) instead of that assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    unsigned long long words[1] = { 0 };
    LLVMConstIntOfArbitraryPrecision(i32, 0, words);

    return 0;
}
