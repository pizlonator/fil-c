/* Fake llvm-c/Core.h for Pizlix (Fil-C).
 *
 * Pure forwarding header: the curated filc_llvm_jit.h is a self-contained
 * substitute for the LLVM-C headers (it must never be included together
 * with them), and it is what the Fil-C runtime exports.  Forwarding here
 * spares us editing every mesa include site. */

#ifndef FILC_FAKE_LLVM_C_CORE_H
#define FILC_FAKE_LLVM_C_CORE_H

#include <filc_llvm_jit.h>

#endif /* FILC_FAKE_LLVM_C_CORE_H */
