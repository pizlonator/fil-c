# Check 64bit APX instructions with pseudo prefixes for encoding

.text
_start:
        #APX REX2
        .irp m, mov, adc, add, and, cmp, or, sbb, sub, test, xor, xchg
        \m %ecx, %r31d
        {load}  \m %ecx, %r31d
        {store} \m %ecx, %r31d
        {load}  \m (%ecx), %r31d
        {store} \m %r31d, (%ecx)
        .endr

        movaps (%r31),%xmm2
        {load} movaps (%r31),%xmm2
        {store} movaps %xmm2, (%r31)

        #APX EVEX promoted from legacy
        .irp m, adc, add, and, cmp, or, sbb, sub, test, xor
        {evex}         \m %ecx, %r31d
        {evex} {load}  \m %ecx, %r31d
        {evex} {store} \m %ecx, %r31d
        {evex} {load}  \m (%ecx), %r31d
        {evex} {store} \m %r31d, (%ecx)
        .endr

        #APX NDD
        .irp m, adc, add, and, or, sbb, sub, xor
        \m %ecx, %r31d, %eax
        {load}  \m %ecx, %r31d, %eax
        {store} \m %ecx, %r31d, %eax
        {load}  \m %r31,(%r8),%r16
        {load}  \m (%r8),%r31,%r16
        {store} \m %r31,(%r8),%r16
        {store} \m (%r8),%r31,%r16
        .endr

        #APX News.
        .irp m, ccmpb, cfcmovb, ctestb
        \m %edx, %eax
        {load}  \m %edx, %eax
        {store} \m %edx, %eax
        {load}  \m (%edx), %eax
        {store} \m %eax, (%edx)
        .endr
