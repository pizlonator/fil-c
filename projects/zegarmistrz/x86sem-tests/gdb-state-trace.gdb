# gdb-state-trace.gdb -- the x86sem suite's gdb single-stepping hardware
# oracle, adapted from the kframework/X86-64-semantics harness's
# scripts/script_3.gdb (which dumps $rax..$r15, $eflags and
# $ymm0..15.v2_int128 after every `si`). This variant emits the exact
# whole-state line format the suite uses everywhere:
#
#   S rip=<16hex> rax=<16hex> ... r15=<16hex> fl=<8hex> ymm0=<128hex> ... ymm15=<128hex>
#
# so its output compares 1:1 against `zegarmistrz --state-trace=` via
# compare-traces.py. The trace line conventions are identical to
# native-trace.c (the ptrace-free oracle this suite uses by default --
# this container cannot ptrace): GPRs natural hex, fl = raw EFLAGS, and
# each ymmN = the 512-bit register as qwords q7..q0 MSW-first, each
# qword big-endian (the register's low qword is the last 16 chars; bits
# 256..511 are the zmm uppers, zeroed by execve and untouched by the
# corpus).
#
# Order matters: `si` FIRST, then process_instr -- every record is the
# state AFTER an instruction, matching the emulator trace. The corpus
# programs end with the appended exit-syscall epilogue: the `si` that
# executes the syscall terminates the process, the following
# process_instr errors out, and batch mode stops the script -- the trace
# ends cleanly after the last completed instruction, exactly like the
# other two oracles. Requires ptrace (unavailable inside some
# containers); select with ZEG_X86SEM_NATIVE=gdb.

set width 0
set height 0
set verbose off
set pagination off
set disassemble-next-line on

define process_instr
  printf "S rip=%016lx rax=%016lx rbx=%016lx rcx=%016lx rdx=%016lx rsi=%016lx rdi=%016lx rbp=%016lx rsp=%016lx", $rip, $rax, $rbx, $rcx, $rdx, $rsi, $rdi, $rbp, $rsp
  printf " r8=%016lx r9=%016lx r10=%016lx r11=%016lx r12=%016lx r13=%016lx r14=%016lx r15=%016lx", $r8, $r9, $r10, $r11, $r12, $r13, $r14, $r15
  printf " fl=%08lx", (unsigned long)$eflags
  printf " ymm0=%016lx%016lx%016lx%016lx%016lx%016lx%016lx%016lx", (unsigned long)$zmm0.v8_int64[7], (unsigned long)$zmm0.v8_int64[6], (unsigned long)$zmm0.v8_int64[5], (unsigned long)$zmm0.v8_int64[4], (unsigned long)$zmm0.v8_int64[3], (unsigned long)$zmm0.v8_int64[2], (unsigned long)$zmm0.v8_int64[1], (unsigned long)$zmm0.v8_int64[0]
  printf " ymm1=%016lx%016lx%016lx%016lx%016lx%016lx%016lx%016lx", (unsigned long)$zmm1.v8_int64[7], (unsigned long)$zmm1.v8_int64[6], (unsigned long)$zmm1.v8_int64[5], (unsigned long)$zmm1.v8_int64[4], (unsigned long)$zmm1.v8_int64[3], (unsigned long)$zmm1.v8_int64[2], (unsigned long)$zmm1.v8_int64[1], (unsigned long)$zmm1.v8_int64[0]
  printf " ymm2=%016lx%016lx%016lx%016lx%016lx%016lx%016lx%016lx", (unsigned long)$zmm2.v8_int64[7], (unsigned long)$zmm2.v8_int64[6], (unsigned long)$zmm2.v8_int64[5], (unsigned long)$zmm2.v8_int64[4], (unsigned long)$zmm2.v8_int64[3], (unsigned long)$zmm2.v8_int64[2], (unsigned long)$zmm2.v8_int64[1], (unsigned long)$zmm2.v8_int64[0]
  printf " ymm3=%016lx%016lx%016lx%016lx%016lx%016lx%016lx%016lx", (unsigned long)$zmm3.v8_int64[7], (unsigned long)$zmm3.v8_int64[6], (unsigned long)$zmm3.v8_int64[5], (unsigned long)$zmm3.v8_int64[4], (unsigned long)$zmm3.v8_int64[3], (unsigned long)$zmm3.v8_int64[2], (unsigned long)$zmm3.v8_int64[1], (unsigned long)$zmm3.v8_int64[0]
  printf " ymm4=%016lx%016lx%016lx%016lx%016lx%016lx%016lx%016lx", (unsigned long)$zmm4.v8_int64[7], (unsigned long)$zmm4.v8_int64[6], (unsigned long)$zmm4.v8_int64[5], (unsigned long)$zmm4.v8_int64[4], (unsigned long)$zmm4.v8_int64[3], (unsigned long)$zmm4.v8_int64[2], (unsigned long)$zmm4.v8_int64[1], (unsigned long)$zmm4.v8_int64[0]
  printf " ymm5=%016lx%016lx%016lx%016lx%016lx%016lx%016lx%016lx", (unsigned long)$zmm5.v8_int64[7], (unsigned long)$zmm5.v8_int64[6], (unsigned long)$zmm5.v8_int64[5], (unsigned long)$zmm5.v8_int64[4], (unsigned long)$zmm5.v8_int64[3], (unsigned long)$zmm5.v8_int64[2], (unsigned long)$zmm5.v8_int64[1], (unsigned long)$zmm5.v8_int64[0]
  printf " ymm6=%016lx%016lx%016lx%016lx%016lx%016lx%016lx%016lx", (unsigned long)$zmm6.v8_int64[7], (unsigned long)$zmm6.v8_int64[6], (unsigned long)$zmm6.v8_int64[5], (unsigned long)$zmm6.v8_int64[4], (unsigned long)$zmm6.v8_int64[3], (unsigned long)$zmm6.v8_int64[2], (unsigned long)$zmm6.v8_int64[1], (unsigned long)$zmm6.v8_int64[0]
  printf " ymm7=%016lx%016lx%016lx%016lx%016lx%016lx%016lx%016lx", (unsigned long)$zmm7.v8_int64[7], (unsigned long)$zmm7.v8_int64[6], (unsigned long)$zmm7.v8_int64[5], (unsigned long)$zmm7.v8_int64[4], (unsigned long)$zmm7.v8_int64[3], (unsigned long)$zmm7.v8_int64[2], (unsigned long)$zmm7.v8_int64[1], (unsigned long)$zmm7.v8_int64[0]
  printf " ymm8=%016lx%016lx%016lx%016lx%016lx%016lx%016lx%016lx", (unsigned long)$zmm8.v8_int64[7], (unsigned long)$zmm8.v8_int64[6], (unsigned long)$zmm8.v8_int64[5], (unsigned long)$zmm8.v8_int64[4], (unsigned long)$zmm8.v8_int64[3], (unsigned long)$zmm8.v8_int64[2], (unsigned long)$zmm8.v8_int64[1], (unsigned long)$zmm8.v8_int64[0]
  printf " ymm9=%016lx%016lx%016lx%016lx%016lx%016lx%016lx%016lx", (unsigned long)$zmm9.v8_int64[7], (unsigned long)$zmm9.v8_int64[6], (unsigned long)$zmm9.v8_int64[5], (unsigned long)$zmm9.v8_int64[4], (unsigned long)$zmm9.v8_int64[3], (unsigned long)$zmm9.v8_int64[2], (unsigned long)$zmm9.v8_int64[1], (unsigned long)$zmm9.v8_int64[0]
  printf " ymm10=%016lx%016lx%016lx%016lx%016lx%016lx%016lx%016lx", (unsigned long)$zmm10.v8_int64[7], (unsigned long)$zmm10.v8_int64[6], (unsigned long)$zmm10.v8_int64[5], (unsigned long)$zmm10.v8_int64[4], (unsigned long)$zmm10.v8_int64[3], (unsigned long)$zmm10.v8_int64[2], (unsigned long)$zmm10.v8_int64[1], (unsigned long)$zmm10.v8_int64[0]
  printf " ymm11=%016lx%016lx%016lx%016lx%016lx%016lx%016lx%016lx", (unsigned long)$zmm11.v8_int64[7], (unsigned long)$zmm11.v8_int64[6], (unsigned long)$zmm11.v8_int64[5], (unsigned long)$zmm11.v8_int64[4], (unsigned long)$zmm11.v8_int64[3], (unsigned long)$zmm11.v8_int64[2], (unsigned long)$zmm11.v8_int64[1], (unsigned long)$zmm11.v8_int64[0]
  printf " ymm12=%016lx%016lx%016lx%016lx%016lx%016lx%016lx%016lx", (unsigned long)$zmm12.v8_int64[7], (unsigned long)$zmm12.v8_int64[6], (unsigned long)$zmm12.v8_int64[5], (unsigned long)$zmm12.v8_int64[4], (unsigned long)$zmm12.v8_int64[3], (unsigned long)$zmm12.v8_int64[2], (unsigned long)$zmm12.v8_int64[1], (unsigned long)$zmm12.v8_int64[0]
  printf " ymm13=%016lx%016lx%016lx%016lx%016lx%016lx%016lx%016lx", (unsigned long)$zmm13.v8_int64[7], (unsigned long)$zmm13.v8_int64[6], (unsigned long)$zmm13.v8_int64[5], (unsigned long)$zmm13.v8_int64[4], (unsigned long)$zmm13.v8_int64[3], (unsigned long)$zmm13.v8_int64[2], (unsigned long)$zmm13.v8_int64[1], (unsigned long)$zmm13.v8_int64[0]
  printf " ymm14=%016lx%016lx%016lx%016lx%016lx%016lx%016lx%016lx", (unsigned long)$zmm14.v8_int64[7], (unsigned long)$zmm14.v8_int64[6], (unsigned long)$zmm14.v8_int64[5], (unsigned long)$zmm14.v8_int64[4], (unsigned long)$zmm14.v8_int64[3], (unsigned long)$zmm14.v8_int64[2], (unsigned long)$zmm14.v8_int64[1], (unsigned long)$zmm14.v8_int64[0]
  printf " ymm15=%016lx%016lx%016lx%016lx%016lx%016lx%016lx%016lx", (unsigned long)$zmm15.v8_int64[7], (unsigned long)$zmm15.v8_int64[6], (unsigned long)$zmm15.v8_int64[5], (unsigned long)$zmm15.v8_int64[4], (unsigned long)$zmm15.v8_int64[3], (unsigned long)$zmm15.v8_int64[2], (unsigned long)$zmm15.v8_int64[1], (unsigned long)$zmm15.v8_int64[0]
  printf "\n"
end

define nstep
  set $n = $arg0
  while ($n--)
    si
    process_instr
  end
end

b *_start
run
nstep 10000
quit
