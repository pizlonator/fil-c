# jit_session_isolation

The Fil-C LLVM JIT shim routes lazy extern
resolution per module (proven here).

* `run_host_isolation.sh` — self-contained host proof. Embeds a C harness,
  builds it with the host `cc` (needs only the internal
  `filc/llvmjit/filc_llvm_jit_api.h` and `dlopen`), and runs it against
  `pizfix/libexec/libfilcllvmjit.so`. Two sessions install different
  resolve callbacks for the same extern name `shared_ext`; each module's
  JITed pizlonated getter must return a flight carrying its own module's
  address, checked against that module's `filc_jit_module` token. Sessions
  are then destroyed while the modules stay callable. Run from the
  repository root:

      sh filc/tests/jit_session_isolation/run_host_isolation.sh

* `jit_session_isolation.c` — `filc/run-tests` driver. Shells out to the
  script above so the proof runs in the normal test pass
  (`filc/run-tests -t jit_session_isolation`).

Background: resolution state lives in the jit module object (resolver
callback plus context, zexact table, recorded roots), never in the
session. `CompileModule` time rewrites each module's blessed declarations
to module-private wrappers carrying that compile's resolve callbacks (see
`bindModuleResolvers` in `filc/llvmjit/filc_llvm_jit_shim.cpp`), and each
module links in its own JITDylib defining the absolute symbol
`filc_jit_module` for its jit module object, so older code can never reach
another module's resolver and sessions may be destroyed once compilation
finishes.
