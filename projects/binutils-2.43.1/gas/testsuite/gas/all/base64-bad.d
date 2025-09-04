#name: .base64 errors test
#source: base64-bad.s
#error_output: base64-bad.l
# The Z80 backend has its own error message for an unterminated string.
#notarget: z80-*
