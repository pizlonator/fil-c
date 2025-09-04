        /* The following two instructions have constant targets that
           fix in the JA 16-bit signed displacement operand.  These
           are not relaxed.  */
1:      ja -32768
        ja 32767
        /* The following instruction refers to a defined symbol that
           is in reach, so it should not be relaxed.  */
        ja 1b
        /* The following instruction has an undefined symbol as a
           target.  It is not to be relaxed.  */
        ja undefined + 10
        /* The following instruction refers to a defined symbol that
           is not in reach, so it shall be relaxed to JAL.  */
        ja tail
        /* Now the symbol is in reach, and the following instruction
           shall not be relaxed.  */
        ja tail
        .space 262136
        tail = .
        /* The jump back is too large and shall be relaxed.  */
        ja 1b
