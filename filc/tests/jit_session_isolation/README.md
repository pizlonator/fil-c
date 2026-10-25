# jit_session_isolation

The Fil-C LLVM JIT shim routes lazy extern
resolution per session (proven here).

* `run_host_isolation.sh` — self-contained host proof. Embeds a C harness,
  builds it with the host `cc` (needs only the internal
  `filc/llvmjit/filc_llvm_jit_api.h` and `dlopen`), and runs it against
  `pizfix/libexec/libfilcllvmjit.so`. Two sessions install different
  resolve callbacks for the same extern name `shared_ext`; each session's
  JITed pizlonated getter must return a flight carrying its own session's
  address. Run from the repository root:

      sh filc/tests/jit_session_isolation/run_host_isolation.sh

* `jit_session_isolation.c` — `filc/run-tests` driver. Shells out to the
  script above so the proof runs in the normal test pass
  (`filc/run-tests -t jit_session_isolation`).

Background: before the fix, `FilLLVMJIT_shim_resolveByName/Raw` scanned
all sessions and handed every JITed module the most-recently-configured
callback/table. The fix rewrites each module's blessed declarations at
`AddModule` time to module-private wrappers carrying the owning session
(see `bindSessionResolvers` in `filc/llvmjit/filc_llvm_jit_shim.cpp`), so
older code can never reach a newer session's callback. The legacy shared
helpers keep last-wins scan semantics for ABI stability (the harness
prints theirs as informational).
