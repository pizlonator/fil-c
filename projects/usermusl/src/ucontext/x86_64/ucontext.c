#include <pizlonated_runtime.h>
#include <stdfil.h>
#include <stdlib.h>
#include <ucontext.h>

int getcontext(ucontext_t *ucp)
{
	ucp->__uc_fiber_context = zfiber_context_new();
	zfiber_context_bind_sigset(ucp->__uc_fiber_context, &ucp->uc_sigmask);
	zfiber_context_getcontext(ucp->__uc_fiber_context);
	ucp->uc_link = NULL;
	ucp->uc_stack.ss_sp = NULL;
	ucp->uc_stack.ss_flags = 0;
	ucp->uc_stack.ss_size = 0;
	return 0;
}

int setcontext(const ucontext_t *ucp)
{
	zfiber_context_setcontext(ucp->__uc_fiber_context);
	return 0;
}

int swapcontext(ucontext_t *oucp, const ucontext_t *ucp)
{
	if (!oucp->__uc_fiber_context) {
		oucp->__uc_fiber_context = zfiber_context_new();
		zfiber_context_bind_sigset(oucp->__uc_fiber_context, &oucp->uc_sigmask);
	}
	zfiber_context_swapcontext(oucp->__uc_fiber_context, ucp->__uc_fiber_context);
	return 0;
}

struct makecontext_args {
	ucontext_t *ucp;
	void (*func)();
	int argc;
	void *first_real_arg;
};

static void makecontext_trampoline(void)
{
	struct makecontext_args *args = (struct makecontext_args *)zcallee_closure_data();
	zcall(args->func, &args->first_real_arg);
	ucontext_t *link = args->ucp->uc_link;
	if (!link)
		exit(0);
	setcontext(link);
}

void makecontext(ucontext_t *ucp, void (*func)(), int argc, ...)
{
	(void)func;
	(void)argc;
	zfiber_context_makecontext(ucp->__uc_fiber_context,
	                           ucp->uc_stack.ss_size,
	                           zclosure_new(makecontext_trampoline, zargs()));
}
