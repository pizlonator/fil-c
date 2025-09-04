# name: MVE vqshr[u]n{b,t} instructions
# as: -march=armv8.1-m.main+mve
# objdump: -dr -marmv8.1-m.main

.*: +file format .*arm.*

Disassembly of section \.text:

[0-9a-f]+ <\.text>:
\s+[0-9a-f]+:\tee88 0f40 \tvqshrnb\.s16\tq0, q0, #8
\s+[0-9a-f]+:\tee8f 0f40 \tvqshrnb\.s16\tq0, q0, #1
\s+[0-9a-f]+:\tee88 0f4e \tvqshrnb\.s16\tq0, q7, #8
\s+[0-9a-f]+:\tee88 ef40 \tvqshrnb\.s16\tq7, q0, #8
\s+[0-9a-f]+:\tee88 1f40 \tvqshrnt\.s16\tq0, q0, #8
\s+[0-9a-f]+:\tfe88 0f40 \tvqshrnb\.u16\tq0, q0, #8
\s+[0-9a-f]+:\tee90 0f40 \tvqshrnb\.s32\tq0, q0, #16
\s+[0-9a-f]+:\tee98 0f40 \tvqshrnb\.s32\tq0, q0, #8
\s+[0-9a-f]+:\tee9f 0f40 \tvqshrnb\.s32\tq0, q0, #1
\s+[0-9a-f]+:\tfe90 0f40 \tvqshrnb\.u32\tq0, q0, #16
\s+[0-9a-f]+:\tee88 0fc0 \tvqshrunb\.s16\tq0, q0, #8
\s+[0-9a-f]+:\tee8f 0fc0 \tvqshrunb\.s16\tq0, q0, #1
\s+[0-9a-f]+:\tee88 0fce \tvqshrunb\.s16\tq0, q7, #8
\s+[0-9a-f]+:\tee88 efc0 \tvqshrunb\.s16\tq7, q0, #8
\s+[0-9a-f]+:\tee88 1fc0 \tvqshrunt\.s16\tq0, q0, #8
\s+[0-9a-f]+:\tee90 0fc0 \tvqshrunb\.s32\tq0, q0, #16
\s+[0-9a-f]+:\tee98 0fc0 \tvqshrunb\.s32\tq0, q0, #8
\s+[0-9a-f]+:\tee9f 0fc0 \tvqshrunb\.s32\tq0, q0, #1
