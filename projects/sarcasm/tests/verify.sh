#!/bin/sh
# Full sarcasm verification. Run from the repo root on the host (lute here; as/clang via
# docker). Covers: behavioral correctness, every compile-time rejection, the presence of
# every inserted check/runtime call (structural), and behavioral memory-safety traps.
# (No `set -e`: reject() intentionally runs sarcasm expecting a non-zero exit.)
DOCKER="docker exec 1b18d1174044"
mkdir -p tests/gen
pass=0; fail=0

# reject FIXTURE EXPECTED-SUBSTRING : sarcasm must reject FIXTURE with a message containing
# EXPECTED-SUBSTRING (clean error, non-zero exit, no stack trace).
reject() {
  out="$(./sarcasm.sh -S -o /dev/null "$1" 2>&1)"; rc=$?
  if [ "$rc" -eq 0 ]; then echo "  FAIL: $1 was not rejected"; fail=$((fail+1))
  elif echo "$out" | grep -q "sarcasm:.*$2"; then echo "  ok: rejects $1 ($2)"; pass=$((pass+1))
  else echo "  FAIL: $1 rejected but message lacked '$2': $out"; fail=$((fail+1)); fi
}
# has FILE PATTERN LABEL : the generated FILE must contain PATTERN (an inserted check/call).
has() {
  if grep -qE "$2" "tests/gen/$1"; then echo "  ok: $3"; pass=$((pass+1))
  else echo "  FAIL: $3 (pattern /$2/ not found in $1)"; fail=$((fail+1)); fi
}

echo "== generating (sarcasm) =="
./sarcasm.sh -S -o tests/gen/t1.s      tests/test-spasm.s
./sarcasm.sh -S -o tests/gen/t2.s      tests/test2-spasm.s
./sarcasm.sh -S -o tests/gen/t3.s      tests/test3-spasm.s
./sarcasm.sh -S -o tests/gen/t30.s     tests/test3-spasm0.s
./sarcasm.sh -S -o tests/gen/store.s   tests/store-spasm.s
./sarcasm.sh -S -o tests/gen/regidx.s  tests/regidx-spasm.s
./sarcasm.sh -S -o tests/gen/nullcap.s tests/nullcap-spasm.s
./sarcasm.sh -S -o tests/gen/nullret.s tests/nullret-spasm.s
./sarcasm.sh -S -o tests/gen/ptrret.s  tests/ptrret-spasm.s
./sarcasm.sh -S -o tests/gen/recurse.s tests/recurse-spasm.s
./sarcasm.sh -S -o tests/gen/spill.s   tests/spill-spasm.s
./sarcasm.sh -S -o tests/gen/mk.s      tests/movkshift-arm.s
./sarcasm.sh -S -o tests/gen/zx.s      tests/zeroext-arm.s
./sarcasm.sh -S -o tests/gen/med.s     tests/medium-arm64.s
./sarcasm.sh -S -o tests/gen/big.s     tests/large-arm64.s
./sarcasm.sh    -S -o tests/gen/t2nog.s tests/test2-spasm.s
./sarcasm.sh -g -S -o tests/gen/t2g.s   tests/test2-spasm.s
for a in alloca-spasm alloca-O0-spasm stackbuf-spasm stackbuf-O0-spasm stackbufs-spasm stackbufs-O0-spasm; do
  ./sarcasm.sh -S -o tests/gen/$a.s tests/$a.s
done

echo "== compile-time rejections =="
reject tests/nosig-spasm.s            "has no ;! signature annotation"
reject tests/badmem-spasm.s           "memory operand on non-load/store"
reject tests/badframe-spasm.s         "caller's argument area"
reject tests/reject-below-frame.s     "stack access below frame"
reject tests/reject-read-outside-frame.s "stack access outside the frame"
reject tests/reject-stack-addr.s      "taking address of stack frame"
reject tests/reject-badsig.s          "cannot parse function signature"
reject tests/reject-too-many-args.s   ">3 register args"
reject tests/reject-alloca-dupsize.s  "duplicate .alloca size"
reject tests/reject-alloca-nosize.s   "has no preceding .alloca size"

echo "== inserted checks / runtime calls present (structural) =="
has t1.s     "b\tfilc_stack_overflow_failure"        "stack-overflow check inserted"
has t1.s     "cmp\tsp, x9"                            "stack-overflow compare inserted"
has t1.s     "bl\tfilc_optimized_access_check_fail"   "bounds/null access check inserted"
has t1.s     "ldrb\tw[0-9]+, \[x[0-9]+, #8\]"          "pollcheck flag load inserted (loop)"
has t1.s     "tst\tw[0-9]+, #14"                       "pollcheck flag test inserted (loop)"
has t1.s     "bl\tfilc_pollcheck_slow"                 "pollcheck slow-call inserted (loop)"
has t1.s     "bl\tfilc_cc_args_check_failure"          "generic-thunk arg-size check inserted"
has store.s  "adrp\tx[0-9]+, :got:filc_current_marking_state" "store barrier marking-state load inserted"
has store.s  "bl\tfilc_store_barrier_for_lower_slow"   "store barrier slow-call inserted"
has store.s  "bl\tfilc_object_ensure_aux_ptr_outline"  "aux-array allocation call inserted"
has store.s  "tst\tx[0-9]+, #0x6000000000000"          "read-only/special object reject inserted"
has t3.s     "tbnz\tw0, #0"                            "call exception-propagation check inserted"
has t3.s     "bl\tfilc_check_function_call_fail"       "callsite resolver function-check fail inserted"
has t3.s     "bl\tfilc_cc_rets_check_failure"          "callsite resolver return-size check inserted"
has spill.s  "(ldr|str)\tx[0-9]+, \[sp, #[0-9]+\]"     "register spilling engaged (stack slots used)"
has alloca-spasm.s "bl\tfilc_allocate"                 "alloca lowered to a filc_allocate GC allocation"
has stackbuf-spasm.s "mov\tx1, #400"                   "fixed-size alloca uses its declared byte size"

echo "== lift/render round-trip (unit) =="
rout="$(lute tests/roundtrip-test.luau 2>&1)"; rrc=$?; echo "$rout"
if [ "$rrc" -ne 0 ] || echo "$rout" | grep -q "MISMATCH"; then
  echo "  FAIL: roundtrip-test.luau"; fail=$((fail+1))
else
  echo "  ok: lift + identity-coloring reproduces the input byte-for-byte"; pass=$((pass+1))
fi

echo "== arch/syntax auto-detection (unit) =="
dout="$(lute tests/detect-test.luau 2>&1)"; echo "$dout"
pass=$((pass + $(echo "$dout" | grep -c '^  ok:')))
fail=$((fail + $(echo "$dout" | grep -c '^  FAIL:')))

echo "== spill reload-elimination cleanup (unit) =="
cout="$(lute tests/cleanup-test.luau 2>&1)"; echo "$cout"
pass=$((pass + $(echo "$cout" | grep -c '^  ok:')))
fail=$((fail + $(echo "$cout" | grep -c '^  FAIL:')))

echo "== assemble + link + run + traps (docker) =="
$DOCKER sh tests/run-features.sh > tests/gen/out.txt 2>&1 || true
cat tests/gen/out.txt
dpass=$(grep -c '^  ok:' tests/gen/out.txt || true)
dfail=$(grep -c '^  FAIL:' tests/gen/out.txt || true)
pass=$((pass + dpass)); fail=$((fail + dfail))
rm -rf tests/gen
echo "TOTAL: $pass passed, $fail failed"
[ "$fail" = 0 ]
