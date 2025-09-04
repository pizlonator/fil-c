        /* The following two instructions have constant targets that
           fix in the jump 16-bit signed displacement operand.  */
1:      jeq %r1, %r2, -32768
        jlt %r1, %r2, 32767
        /* The following instructions refer to defined symbols that
           are in reach, so they should not be relaxed.  */
        jle %r1, %r2, 1b
        jeq %r1, %r2, tail
        jgt %r1, %r2, tail
        /* The following instructions refers to defined symbols that
           are not in reach, so they shall be relaxied.  */
        .space 262136
        tail = .
        jne %r1, 0, 1b
