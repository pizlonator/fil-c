/* Proposed WG14 improved signals support
(C) 2025 Niall Douglas <http://www.nedproductions.biz/>
File Created: Feb 2025


Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License in the accompanying file
Licence.txt or at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "wg14_signals/thrd_signal_handle.h"

#include "thrd_signal_handle_common.ipp"

#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>

#if WG14_SIGNALS_HAVE__SETJMP
#define WG14_SIGNALS_SETJMP _setjmp
#define WG14_SIGNALS_LONGJMP _longjmp
#else
#define WG14_SIGNALS_SETJMP setjmp
#define WG14_SIGNALS_LONGJMP longjmp
#endif

__attribute__((constructor)) const sigset_t *
WG14_SIGNALS_PREFIX(synchronous_sigset)(void)
{
  static sigset_t v;
  static const int signos[] = {SIGABRT, SIGBUS,  SIGFPE, SIGILL,
                               SIGPIPE, SIGSEGV, SIGSYS};
  if(sigismember(&v, signos[0]))
  {
    return &v;
  }
  sigset_t x;
  sigemptyset(&x);
  for(size_t n = 0; n < sizeof(signos) / sizeof(signos[0]); n++)
  {
    sigaddset(&x, signos[n]);
  }
  v = x;
  return &v;
}

__attribute__((constructor)) const sigset_t *
WG14_SIGNALS_PREFIX(asynchronous_nondebug_sigset)(void)
{
  static sigset_t v;
  static const int signos[] = {SIGALRM, SIGCHLD, SIGCONT,  SIGHUP,  SIGINT,
                               SIGKILL, SIGSTOP, SIGTERM,  SIGTSTP, SIGTTIN,
                               SIGTTOU, SIGUSR1, SIGUSR2,
#ifdef SIGPOLL
                               SIGPOLL,
#endif
                               SIGPROF, SIGURG,  SIGVTALRM};
  if(sigismember(&v, signos[0]))
  {
    return &v;
  }
  sigset_t x;
  sigemptyset(&x);
  for(size_t n = 0; n < sizeof(signos) / sizeof(signos[0]); n++)
  {
    sigaddset(&x, signos[n]);
  }
  v = x;
  return &v;
}

__attribute__((constructor)) const sigset_t *
WG14_SIGNALS_PREFIX(asynchronous_debug_sigset)(void)
{
  static sigset_t v;
  static const int signos[] = {SIGQUIT, SIGTRAP, SIGXCPU, SIGXFSZ};
  if(sigismember(&v, signos[0]))
  {
    return &v;
  }
  sigset_t x;
  sigemptyset(&x);
  for(size_t n = 0; n < sizeof(signos) / sizeof(signos[0]); n++)
  {
    sigaddset(&x, signos[n]);
  }
  v = x;
  return &v;
}

#if 0
// Reset the SIGABRT handler, and call abort()
static void __attribute__((noreturn)) default_abort(void)
{
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_DFL;
  (void) sigaction(SIGABRT, &sa, WG14_SIGNALS_NULLPTR);
  abort();
}
#endif


// Invoke a sigaction as if it were the first signal handler
static bool invoke_sigaction(const struct sigaction *sa, const int signo,
                             siginfo_t *siginfo, void *context)
{
  if((sa->sa_flags & SA_SIGINFO) != 0)
  {
    sa->sa_sigaction(signo, siginfo, context);
    return true;
  }
  if(sa->sa_handler == SIG_DFL)
  {
    switch(signo)
    {
    case SIGCHLD:
    case SIGURG:
#ifdef SIGWINCH
    case SIGWINCH:
#endif
      // The default is to ignore
      return false;
    default:
    {
      // The default is to abort, so reset the signal handler
      // to default and send ourselves the signal
      struct sigaction sa;
      memset(&sa, 0, sizeof(sa));
      sa.sa_handler = SIG_DFL;
      (void) sigaction(signo, &sa, WG14_SIGNALS_NULLPTR);
      (void) pthread_kill(pthread_self(), signo);
      return true;
    }
    }
  }
  if(sa->sa_handler == SIG_IGN)
  {
    // We do nothing
    return false;
  }
  sa->sa_handler(signo);
  return true;
}

static void prepare_rsi(struct WG14_SIGNALS_PREFIX(thrd_raised_signal_info) *
                        rsi,
                        const int signo, siginfo_t *siginfo, void *context)
{
  rsi->signo = signo;
  rsi->raw_context = context;
  if(siginfo != WG14_SIGNALS_NULLPTR)
  {
    rsi->raw_info = siginfo;
    rsi->error_code = siginfo->si_errno;
    rsi->addr = siginfo->si_addr;
  }
}

// The base signal handler for POSIX
// You must NOT do anything async signal unsafe in here!
static void raw_signal_handler(int signo, siginfo_t *siginfo, void *context)
{
  if(!WG14_SIGNALS_PREFIX(thrd_signal_raise)(signo, siginfo, context))
  {
    // It shouldn't happen that this handler gets called when we have no
    // knowledge of the signal. We could default_abort() here, but it seems more
    // reliable to remove the installation of ourselves and pass on the signal
    // there
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    invoke_sigaction(&sa, signo, siginfo, context);
  }
}

union WG14_SIGNALS_PREFIX(thrd_raised_signal_info_value)
WG14_SIGNALS_PREFIX(thrd_signal_invoke)(
const sigset_t *signals, WG14_SIGNALS_PREFIX(thrd_signal_func_t) guarded,
WG14_SIGNALS_PREFIX(thrd_signal_recover_t) recovery,
WG14_SIGNALS_PREFIX(thrd_signal_decide_t) decider,
union WG14_SIGNALS_PREFIX(thrd_raised_signal_info_value) value)
{
  if(signals == WG14_SIGNALS_NULLPTR || guarded == WG14_SIGNALS_NULLPTR ||
     decider == WG14_SIGNALS_NULLPTR)
  {
    abort();
  }
  if(0 != thrd_signal_global_tss_state_init())
  {
    union WG14_SIGNALS_PREFIX(thrd_raised_signal_info_value) ret;
    ret.int_value = -1;
    return ret;
  }
  struct thrd_signal_global_state_tss_state_t *tss =
  thrd_signal_global_tss_state();
  struct thrd_signal_global_state_tss_state_per_frame_t *old = tss->front,
                                                        current;
  memset(&current, 0, sizeof(current));
  current.prev = old;
  current.guarded = signals;
  current.recovery = recovery;
  current.decider = decider;
  current.rsi.value = value;
  tss->front = &current;
  if(WG14_SIGNALS_SETJMP(current.buf) != 0)
  {
    tss->front = old;
    // Technically needed to ensure previous handler is active before recovery
    // function is called, as it may raise a signal
    atomic_signal_fence(memory_order_acq_rel);
    return recovery(&current.rsi);
  }
  // Technically needed to ensure setjmp buffer written out before guarded
  // function is called
  atomic_signal_fence(memory_order_acq_rel);
  union WG14_SIGNALS_PREFIX(thrd_raised_signal_info_value) ret = guarded(value);
  tss->front = old;
  atomic_signal_fence(memory_order_acq_rel);
  return ret;
}

// You must NOT do anything async signal unsafe in here!
bool WG14_SIGNALS_PREFIX(thrd_signal_raise)(int signo, void *raw_info,
                                            void *raw_context)
{
  if(0 != thrd_signal_global_tss_state_init())
  {
    return false;
  }
  if(signo == 0)
  {
    // Caller is doing the non-async safe setup
    return false;
  }
  struct thrd_signal_global_state_tss_state_t *tss =
  thrd_signal_global_tss_state();
  struct thrd_signal_global_state_tss_state_per_frame_t *frame = tss->front;
  while(frame != WG14_SIGNALS_NULLPTR)
  {
    if(sigismember(frame->guarded, signo))
    {
      prepare_rsi(&frame->rsi, signo, (siginfo_t *) raw_info, raw_context);
      switch(frame->decider(&frame->rsi))
      {
      case WG14_SIGNALS_PREFIX(thrd_signal_decision_next_decider):
        break;
      case WG14_SIGNALS_PREFIX(thrd_signal_decision_resume_execution):
        frame = frame->prev;
        return true;
      case WG14_SIGNALS_PREFIX(thrd_signal_decision_invoke_recovery):
        if(frame->recovery == WG14_SIGNALS_NULLPTR)
        {
          frame = frame->prev;
          return true;
        }
        WG14_SIGNALS_LONGJMP(frame->buf, 1);
      }
    }
    frame = frame->prev;
  }

  struct WG14_SIGNALS_PREFIX(thrd_signal_global_state_t) *state =
  WG14_SIGNALS_PREFIX(thrd_signal_global_state)();
  LOCK(state->lock);
  signo_to_sighandler_map_t_itr it =
  signo_to_sighandler_map_t_get(&state->signo_to_sighandler_map, signo);
  if(signo_to_sighandler_map_t_is_end(it))
  {
    // We don't have a handler installed for that signal
    UNLOCK(state->lock);
    return false;
  }
  struct sigaction sa = signo_to_sighandler_map_t_value(it)->old_handler;
  struct WG14_SIGNALS_PREFIX(thrd_raised_signal_info) rsi;
  prepare_rsi(&rsi, signo, (siginfo_t *) raw_info, raw_context);
  if(signo_to_sighandler_map_t_value(it)->global_handler.front !=
     WG14_SIGNALS_NULLPTR)
  {
    struct global_signal_decider_t *current =
    signo_to_sighandler_map_t_value(it)->global_handler.front;
    do
    {
      rsi.value = current->value;
      UNLOCK(state->lock);
      if(current->decider(&rsi))
      {
        return true;
      }
      LOCK(state->lock);
      current = current->next;
    } while(current != WG14_SIGNALS_NULLPTR);
  }
  // None of our deciders want this, so call previously installed signal handler
  UNLOCK(state->lock);
  invoke_sigaction(&sa, signo, (siginfo_t *) raw_info, raw_context);
  return true;
}

static bool install_sighandler_impl(struct sighandler_t *item, const int signo)
{
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = raw_signal_handler;
  sa.sa_flags = SA_SIGINFO | SA_NOCLDWAIT | SA_NODEFER;
  if(-1 == sigaction(signo, &sa, &item->old_handler))
  {
    return false;
  }
  return true;
}

static bool uninstall_sighandler_impl(struct sighandler_t *item,
                                      const int signo)
{
  (void) sigaction(signo, &item->old_handler, WG14_SIGNALS_NULLPTR);
  return true;
}
