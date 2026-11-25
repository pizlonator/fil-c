#define _GNU_SOURCE
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <ucontext.h>

/* Compile-time verification of every offset used in ucontext.s (pure C99). */
#define UCONTEXT_ASSERT(name, cond) typedef char ucontext_assert_##name[(cond) ? 1 : -1]

UCONTEXT_ASSERT(off_r8,      offsetof(ucontext_t, uc_mcontext.gregs[REG_R8])  == 40);
UCONTEXT_ASSERT(off_r9,      offsetof(ucontext_t, uc_mcontext.gregs[REG_R9])  == 48);
UCONTEXT_ASSERT(off_r12,     offsetof(ucontext_t, uc_mcontext.gregs[REG_R12]) == 72);
UCONTEXT_ASSERT(off_r13,     offsetof(ucontext_t, uc_mcontext.gregs[REG_R13]) == 80);
UCONTEXT_ASSERT(off_r14,     offsetof(ucontext_t, uc_mcontext.gregs[REG_R14]) == 88);
UCONTEXT_ASSERT(off_r15,     offsetof(ucontext_t, uc_mcontext.gregs[REG_R15]) == 96);
UCONTEXT_ASSERT(off_rdi,     offsetof(ucontext_t, uc_mcontext.gregs[REG_RDI]) == 104);
UCONTEXT_ASSERT(off_rsi,     offsetof(ucontext_t, uc_mcontext.gregs[REG_RSI]) == 112);
UCONTEXT_ASSERT(off_rbp,     offsetof(ucontext_t, uc_mcontext.gregs[REG_RBP]) == 120);
UCONTEXT_ASSERT(off_rbx,     offsetof(ucontext_t, uc_mcontext.gregs[REG_RBX]) == 128);
UCONTEXT_ASSERT(off_rdx,     offsetof(ucontext_t, uc_mcontext.gregs[REG_RDX]) == 136);
UCONTEXT_ASSERT(off_rcx,     offsetof(ucontext_t, uc_mcontext.gregs[REG_RCX]) == 152);
UCONTEXT_ASSERT(off_rsp,     offsetof(ucontext_t, uc_mcontext.gregs[REG_RSP]) == 160);
UCONTEXT_ASSERT(off_rip,     offsetof(ucontext_t, uc_mcontext.gregs[REG_RIP]) == 168);
UCONTEXT_ASSERT(off_fpregs,  offsetof(ucontext_t, uc_mcontext.fpregs)         == 224);
UCONTEXT_ASSERT(off_sigmask, offsetof(ucontext_t, uc_sigmask)                 == 296);
UCONTEXT_ASSERT(off_fpmem,   offsetof(ucontext_t, __fpregs_mem)               == 424);
UCONTEXT_ASSERT(off_mxcsr,   offsetof(ucontext_t, __fpregs_mem) + offsetof(struct _fpstate, mxcsr) == 448);
UCONTEXT_ASSERT(sig_block,   SIG_BLOCK == 0);
UCONTEXT_ASSERT(sig_setmask, SIG_SETMASK == 2);

extern void __yolomusl_start_context(void);

void makecontext(ucontext_t *ucp, void (*func)(), int argc, ...)
{
	uintptr_t *sp;
	va_list ap;
	int i;
	int extra = argc > 6 ? argc - 6 : 0;
	int slots = 1 + extra + 1;

	/* Ensure %rsp % 16 == 8 on entry to func per System V AMD64 ABI. */
	if ((slots & 1) == 0)
		slots += 1;

	sp = (uintptr_t *)(((uintptr_t)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size) & ~(uintptr_t)15);
	sp -= slots;

	sp[0] = (uintptr_t)&__yolomusl_start_context;
	sp[1 + extra] = (uintptr_t)ucp->uc_link;

	ucp->uc_mcontext.gregs[REG_RIP] = (greg_t)(uintptr_t)func;
	ucp->uc_mcontext.gregs[REG_RSP] = (greg_t)(uintptr_t)sp;
	ucp->uc_mcontext.gregs[REG_RBX] = (greg_t)(uintptr_t)&sp[1 + extra];

	va_start(ap, argc);
	for (i = 0; i < argc; i++) {
		greg_t val = va_arg(ap, greg_t);
		switch (i) {
		case 0:
			ucp->uc_mcontext.gregs[REG_RDI] = val;
			break;
		case 1:
			ucp->uc_mcontext.gregs[REG_RSI] = val;
			break;
		case 2:
			ucp->uc_mcontext.gregs[REG_RDX] = val;
			break;
		case 3:
			ucp->uc_mcontext.gregs[REG_RCX] = val;
			break;
		case 4:
			ucp->uc_mcontext.gregs[REG_R8] = val;
			break;
		case 5:
			ucp->uc_mcontext.gregs[REG_R9] = val;
			break;
		default:
			sp[1 + (i - 6)] = (uintptr_t)val;
			break;
		}
	}
	va_end(ap);
}
