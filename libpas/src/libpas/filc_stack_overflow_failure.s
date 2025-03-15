# Copyright (c) 2024 Epic Games, Inc. All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY EPIC GAMES, INC. ``AS IS AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL EPIC GAMES, INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

	.text
	.globl filc_stack_overflow_failure
	.p2align 4, 0x90
	.type filc_stack_overflow_failure,@function
        
        # This special function is jmped to by the compiler-generated stack height check. Because we
        # might get here with %rsp already pointing out of bounds of available stack memory, we cannot
        # use the stack. So, we have a global failure_stack that we protect with a lock.
filc_stack_overflow_failure:
        
        # Grab the lock that protects the failure_stack. This has to be a spinlock and it can be a
        # spinlock. It has to be a spinlock because we're not on a valid stack right now, so we cannot
        # do anything complicated. Maybe we could do syscalls, but even that's sketchy. It can be a
        # spinlock because the whole point is we'll panic anyway, so perf doesn't matter.
	movb $1, %cl
.Lretry_lock:
	xorl %eax, %eax
	lock cmpxchgb %cl, lock(%rip)
	jne .Lretry_lock
        
        # Set up the failure_stack as our stack, just so we can call into the runtime to create a
        # Fil-C panic. Note that the $8192 constant has to match the failure_stack's size, below.
	leaq failure_stack(%rip), %rax
        addq $8192, %rax
        movq %rax, %rsp
        
        # Now create the Fil-C panic.
	call filc_stack_overflow_failure_impl@PLT

        # This is the simple spinlock we use to protect the failure_stack.
	.type lock,@object
	.local lock
	.comm lock,1,1

        # We execute the panic using this buffer as our stack. Note that the 8192 size has to match
        # what we add, above.
	.type failure_stack,@object
	.local failure_stack
	.comm failure_stack,8192,16

        .section    .note.GNU-stack,"",@progbits
        
