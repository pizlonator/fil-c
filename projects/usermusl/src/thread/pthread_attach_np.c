#define _GNU_SOURCE
#include "pthread_impl.h"
#include "libc.h"
#include "lock.h"

/* A program that embeds Fil-C can run Fil-C code on its own native threads
 * by attaching them to the Fil-C runtime (filc_thread_attach_current). Such a
 * thread has no struct pthread, since pthread_create never ran for it; these
 * give it one, and take it away again. They're called from the attached
 * thread itself, entered, with no Fil-C frames below the call. */

int pthread_attach_current_np(void)
{
	struct pthread *new, *anchor;
	sigset_t set;

	if (zthread_self_cookie()) return EEXIST;
	if (!libc.can_do_threads) return ENOSYS;

	new = zgc_alloc(sizeof(struct pthread));
	new->self = new;
	new->zthread = zthread_self();
	new->tsd = zgc_alloc(__pthread_tsd_size);
	new->locale = &libc.global_locale;
	/* Nobody can join a thread that pthread_create didn't make, and it
	 * can't exit through pthread_exit, so cancellation starts disabled. */
	new->detach_state = DT_DETACHED;
	new->canceldisable = 1;
	new->attached = 1;
	new->robust_list.head = &new->robust_list.head;
	new->sysinfo = __sysinfo;

	/* As in pthread_create, before taking the thread list lock. If this is
	 * the process's second thread, the first one (the anchor) can't be
	 * changing the list, as it's the only one that could. */
	if (!libc.threaded) __pthread_go_threaded(__thread_list_anchor);

	__block_app_sigs(&set);

	/* From here, __pthread_self() works, which the thread list lock needs. */
	zthread_set_self_cookie(new);
	new->tid = zthread_self_id();

	__tl_lock();
	anchor = __thread_list_anchor;
	new->canary = anchor->canary;
	if (!libc.threads_minus_1++) libc.need_locks = 1;
	new->next = anchor->next;
	new->prev = anchor;
	new->next->prev = new;
	new->prev->next = new;
	__tl_unlock();

	__restore_sigs(&set);
	return 0;
}

int pthread_detach_current_np(void)
{
	pthread_t self = __pthread_self();
	sigset_t set;
	int last;

	if (!self || !self->attached) return EINVAL;
	if (self->cancelbuf) return EBUSY;

	/* The thread list can't become empty: there would be no thread to
	 * anchor the next one that attaches. */
	__block_app_sigs(&set);
	__tl_lock();
	last = self->next == self;
	__tl_unlock();
	__restore_sigs(&set);
	if (last) return EBUSY;

	/* What a thread does on its way out, minus the cancellation handlers,
	 * which can only be pushed by frames that are still below us. */
	__pthread_tsd_run_dtors();

	__block_app_sigs(&set);
	LOCK(self->killlock);
	__tl_lock();
	if (self->next == self) {
		/* Every other thread left meanwhile. */
		__tl_unlock();
		UNLOCK(self->killlock);
		__restore_sigs(&set);
		return EBUSY;
	}
	__pthread_unlist_self(self);
	self->attached = 0;
	__restore_sigs(&set);
	/* The cookie still points at self, now unlisted, for signal handlers
	 * that run before the thread detaches from the runtime, which clears it. */
	return 0;
}
