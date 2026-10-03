# D9 fixed-frame escape promotion pinned for the push-alias RMW (ALU/unary)
# RECOVERY paths: a promoted frame whose callsite passes the region pointer as
# its 7th (STACK-PASSED) argument whose outgoing word 0 IS the save slot of an
# outstanding `pushq %r12` — but the aliased access that defines the parked
# register's web is NOT a mov store. The alias model rewrites EVERY aliased
# (slot-aligned, full-width, dest-first, exactly-classifying) access KEEPING
# ITS MNEMONIC onto the pushed register, so a mem-dest ALU RMW
# (`addq %rbx, (%rsp)` — web := 900 + V) and a unary form (`notq (%rsp)` —
# web := ~900) define the web exactly like a different-source mov store does.
# The whole shape is HARDWARE-EXACT — and accepted — because NOTHING reads the
# pushed register while the web and the hardware register disagree: in
# hardware neither the push nor any store to the slot ever modifies %r12 (it
# keeps the pushed 900), and in the model every web-defining access keeps the
# web EQUAL to the slot's content (the RMW computes slot := slot OP src and
# web := web OP src from the same operands, starting equal). So the marshal's
# word 0 (served from the web), every aliased slot access, and the dropped
# pop's reload (web := slot content; hardware reloads the same bytes) all hold
# ONE value on both sides — the pad's word is the outgoing stack arg and the
# popped web is pinned, and the divergence window closes at the pop with model
# and hardware re-synced. The poisoned-window pass only rejects a window in
# which a statement DIRECTLY names the pushed register; these two functions
# have none (that shape is sarcasm-frame-escape-push-alias-rmw-reject-att).
# The escape cluster (`leaq 16(%rsp), %rax` + offset leas + `call sink8 ;!
# long(ptr,...)`) promotes the whole fixed frame to ONE GC allocation
# (filc_allocate in the sarcasm output — expected here); the outgoing band is
# carved out below the promoted region and stays slot traffic. The sink writes
# a distinct tag through each pointer argument, stores the marshalled word 0
# (a long) through the last one, and returns the tag sum; the asm reads its
# frame back through direct region slots and parked pointers, re-derives the
# two pinned 900s address-independently (each is subtracted from the region
# pointer / compared against the restored web), and returns an exact checksum
# (2262, hardware-exact vs a native gcc build of the same asm semantics).
	.text
	.globl	rmwok
	.type	rmwok, @function
rmwok:                          #! long(ptr,ptr,long)
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	subq	$104, %rsp          # D0 = 144; region [16,104)

	# args: %rdi = bufA, %rsi = bufB, %rdx = the seed
	movq	%rdx, %r12          # park the seed 900 (rdx is a call-clobbered arg reg)
	movq	%rdi, %r13          # park bufA
	movq	%rsi, %r14          # park bufB

	# --- escape cluster: the lea escapes to sink8 -> the whole fixed frame
	# is promoted to ONE GC region [16,104) ---
	leaq	16(%rsp), %rax      # region+0 — the ESCAPING lea
	movq	%rax, %rbx          # park the region+0 pointer V (call-safe)

	pushq	%r12                # save slot IS outgoing word 0; hw r12 = 900
	addq	%rbx, (%rsp)        # ALIASED RMW OPENER: slot = 900+V; web(r12) = 900+V
	                   	    # NO in-window register read: native r12 stays 900 and
	                   	    # nothing observes it until after the pop
	movq	%r14, %rdi          # arg1 = bufB
	movq	%r13, %rsi          # arg2 = bufA
	movq	%rbx, %rdx          # arg3 = region+0
	leaq	32(%rsp), %rcx      # arg4 = region+8
	leaq	40(%rsp), %r8       # arg5 = region+16
	leaq	48(%rsp), %r9       # arg6 = region+24
	movq	%r13, 8(%rsp)       # outgoing word 1 (arg8) = bufA
	                   	    # outgoing word 0 (arg7) = the slot content itself
	                   	    # (no store): the marshal serves web(r12) = 900+V,
	                   	    # hardware serves the slot's 900+V — one value
	call	sink8 ;! long(ptr,ptr,ptr,ptr,ptr,ptr,long,ptr)

	popq	%r12                # dropped pop: web(r12) = slot = 900+V; hw r12 = 900+V

	# --- the popped web is pinned: %r12 - %rbx = 900 on both sides ---
	movq	%rax, %r15          # the sink's tag sum (231)
	movq	%r12, %rax          # 900+V (model web; hardware pop reload — one value)
	subq	%rbx, %rax          # -V -> 900 (V is address-independent: both sides)

	# --- read back the callee's tags (the region survived the call) ---
	movq	(%r14), %rcx        # bufB[0] = tag 11
	addq	%rcx, %rax
	movq	8(%r13), %rcx       # bufA[1] = tag 22
	addq	%rcx, %rax
	movq	32(%rsp), %rcx      # region+16 (c[2]) = tag 33
	addq	%rcx, %rax
	movq	48(%rsp), %rcx      # region+32 (d[3]) = tag 44
	addq	%rcx, %rax
	movq	64(%rsp), %rcx      # region+48 (e[4]) = tag 55
	addq	%rcx, %rax
	movq	80(%rsp), %rcx      # region+64 (f[5]) = tag 66
	addq	%rcx, %rax

	# --- the MARSHALLED pad word: the callee stored arg7 (900+V) at bufA[6];
	# re-deriving it address-independently pins the marshal exactly ---
	movq	48(%r13), %rcx      # 900+V (the web the marshal served)
	subq	%rbx, %rcx          # -> 900
	addq	%rcx, %rax
	addq	%r15, %rax          # + the sink's tag sum: 231 + 231 + 900 + 900 = 2262

	addq	$104, %rsp
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	ret
	.size	rmwok, .-rmwok

	.globl	rmwok2
	.type	rmwok2, @function
rmwok2:                         #! long(ptr,ptr,long)
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	subq	$104, %rsp          # D0 = 144; region [16,104)

	movq	%rdx, %r12          # park the seed 900
	movq	%rdi, %r13          # park bufA
	movq	%rsi, %r14          # park bufB

	leaq	16(%rsp), %rax      # region+0 — the ESCAPING lea
	movq	%rax, %rbx          # rbx = region pointer V

	pushq	%r12                # save slot IS outgoing word 0; hw r12 = 900
	notq	(%rsp)              # ALIASED UNARY OPENER: slot = ~900; web(r12) = ~900
	                   	    # NO in-window register read: the divergence window
	                   	    # closes before anything observes %r12
	movq	%r14, %rdi          # arg1 = bufB
	movq	%r13, %rsi          # arg2 = bufA
	movq	%rbx, %rdx          # arg3 = region+0
	leaq	32(%rsp), %rcx      # arg4 = region+8
	leaq	40(%rsp), %r8       # arg5 = region+16
	leaq	48(%rsp), %r9       # arg6 = region+24
	notq	(%rsp)              # in-window aliased UNARY restore: slot = 900 again,
	                   	    # web(r12) = 900 = the pushed value — re-synced early;
	                   	    # it names no register in its own operands, so the
	                   	    # scan never flags it (it opens its own clean window)
	movq	%r13, 8(%rsp)       # outgoing word 1 (arg8) = bufA
	                   	    # outgoing word 0 (arg7) = the restored slot content (900)
	call	sink8 ;! long(ptr,ptr,ptr,ptr,ptr,ptr,long,ptr)

	popq	%r12                # dropped pop: web(r12) = slot = 900 = hw r12

	# --- the popped web IS the pushed value here: no address arithmetic needed ---
	movq	%rax, %r15          # the sink's tag sum (231)
	movq	%r12, %rax          # 900 — web, hardware pop reload, and pushed value
	addq	%r15, %rax          # + 231

	# --- read back the callee's tags ---
	movq	(%r14), %rcx        # bufB[0] = 11
	addq	%rcx, %rax
	movq	8(%r13), %rcx       # bufA[1] = 22
	addq	%rcx, %rax
	movq	32(%rsp), %rcx      # region+16 = 33
	addq	%rcx, %rax
	movq	48(%rsp), %rcx      # region+32 = 44
	addq	%rcx, %rax
	movq	64(%rsp), %rcx      # region+48 = 55
	addq	%rcx, %rax
	movq	80(%rsp), %rcx      # region+64 = 66
	addq	%rcx, %rax

	# --- the marshalled pad word: arg7 = the restored web (900) ---
	movq	48(%r13), %rcx      # bufA[6] = 900
	addq	%rcx, %rax          # checksum = 900 + 231 + 231 + 900 = 2262

	addq	$104, %rsp
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	ret
	.size	rmwok2, .-rmwok2
	.section	.note.GNU-stack,"",@progbits
