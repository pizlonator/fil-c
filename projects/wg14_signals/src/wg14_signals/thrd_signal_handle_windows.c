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

#include <Windows.h>

#include "thrd_signal_handle_common.ipp"

const sigset_t *WG14_SIGNALS_PREFIX(synchronous_sigset)(void)
{
  static sigset_t v;
  static const int signos[] = {SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGSEGV};
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

const sigset_t *WG14_SIGNALS_PREFIX(asynchronous_nondebug_sigset)(void)
{
  static sigset_t v;
  static const int signos[] = {SIGINT, SIGKILL, SIGSTOP, SIGTERM};
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

const sigset_t *WG14_SIGNALS_PREFIX(asynchronous_debug_sigset)(void)
{
  static sigset_t v;
  return &v;
}


static DWORD win32_exception_code_from_signal(int c)
{
  switch(c)
  {
  default:
    abort();
  case SIGABRT:
    return ((unsigned long) 0xC0000025L) /*EXCEPTION_NONCONTINUABLE_EXCEPTION*/;
  case SIGBUS:
    return ((unsigned long) 0xC0000006L) /*EXCEPTION_IN_PAGE_ERROR*/;
  case SIGILL:
    return ((unsigned long) 0xC000001DL) /*EXCEPTION_ILLEGAL_INSTRUCTION*/;
  // case signalc::interrupt:
  //  return SIGINT;
  // case signalc::broken_pipe:
  //  return SIGPIPE;
  case SIGSEGV:
    return ((unsigned long) 0xC0000005L) /*EXCEPTION_ACCESS_VIOLATION*/;
  case SIGFPE:
    return ((unsigned long) 0xC0000090L) /*EXCEPTION_FLT_INVALID_OPERATION*/;
  }
}
static int signal_from_win32_exception_code(DWORD c)
{
  switch(c)
  {
  case((unsigned long) 0xC0000025L) /*EXCEPTION_NONCONTINUABLE_EXCEPTION*/:
    return SIGABRT;
  case((unsigned long) 0xC0000006L) /*EXCEPTION_IN_PAGE_ERROR*/:
    return SIGBUS;
  case((unsigned long) 0xC000001DL) /*EXCEPTION_ILLEGAL_INSTRUCTION*/:
    return SIGILL;
  // case SIGINT:
  //  return signalc::interrupt;
  // case SIGPIPE:
  //  return signalc::broken_pipe;
  case((unsigned long) 0xC0000005L) /*EXCEPTION_ACCESS_VIOLATION*/:
    return SIGSEGV;
  case((unsigned long) 0xC000008DL) /*EXCEPTION_FLT_DENORMAL_OPERAND*/:
  case((unsigned long) 0xC000008EL) /*EXCEPTION_FLT_DIVIDE_BY_ZERO*/:
  case((unsigned long) 0xC000008FL) /*EXCEPTION_FLT_INEXACT_RESULT*/:
  case((unsigned long) 0xC0000090L) /*EXCEPTION_FLT_INVALID_OPERATION*/:
  case((unsigned long) 0xC0000091L) /*EXCEPTION_FLT_OVERFLOW*/:
  case((unsigned long) 0xC0000092L) /*EXCEPTION_FLT_STACK_CHECK*/:
  case((unsigned long) 0xC0000093L) /*EXCEPTION_FLT_UNDERFLOW*/:
    return SIGFPE;
  default:
    return 0;
  }
}

static void prepare_rsi(struct WG14_SIGNALS_PREFIX(thrd_raised_signal_info) *
                        rsi,
                        const int signo, EXCEPTION_POINTERS *ptrs)
{
  memset(rsi, 0, sizeof(*rsi));
  rsi->signo = signo;
  if(ptrs->ExceptionRecord->NumberParameters >= 2 &&
     ptrs->ExceptionRecord
     ->ExceptionInformation[ptrs->ExceptionRecord->NumberParameters - 2] ==
     (ULONG_PTR) 0xdeadbeefdeadbeef)
  {
    rsi->raw_context =
    (void *) ptrs->ExceptionRecord
    ->ExceptionInformation[ptrs->ExceptionRecord->NumberParameters - 1];
  }
  else
  {
    rsi->raw_context = (void *) ptrs->ContextRecord;
  }
  rsi->raw_info = (void *) ptrs->ExceptionRecord;
  rsi->error_code = (WG14_SIGNALS_PREFIX(thrd_raised_signal_error_code_t))
                    ptrs->ExceptionRecord->ExceptionInformation[2];  // NTSTATUS
  rsi->addr = (void *) ptrs->ExceptionRecord->ExceptionInformation[1];
}

static long win32_exception_filter(
struct WG14_SIGNALS_PREFIX(thrd_raised_signal_info) * rsi,
const sigset_t *guarded, const int signo,
WG14_SIGNALS_PREFIX(thrd_signal_recover_t) recovery,
WG14_SIGNALS_PREFIX(thrd_signal_decide_t) decider,
union WG14_SIGNALS_PREFIX(thrd_raised_signal_info_value) value,
EXCEPTION_POINTERS *ptrs)
{
  if(sigismember(guarded, signo))
  {
    prepare_rsi(rsi, signo, ptrs);
    rsi->value = value;
    switch(decider(rsi))
    {
    case WG14_SIGNALS_PREFIX(thrd_signal_decision_next_decider):
      break;
    case WG14_SIGNALS_PREFIX(thrd_signal_decision_resume_execution):
      return EXCEPTION_CONTINUE_EXECUTION;
    case WG14_SIGNALS_PREFIX(thrd_signal_decision_invoke_recovery):
      return (recovery != WG14_SIGNALS_NULLPTR) ? EXCEPTION_EXECUTE_HANDLER :
                                                  EXCEPTION_CONTINUE_EXECUTION;
    }
  }
  return EXCEPTION_CONTINUE_SEARCH;
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
  struct WG14_SIGNALS_PREFIX(thrd_raised_signal_info) rsi;
  __try
  {
    return guarded(value);
  }
  __except(win32_exception_filter(
  &rsi, signals, signal_from_win32_exception_code(GetExceptionCode()), recovery,
  decider, value, GetExceptionInformation()))
  {
    return recovery(&rsi);
  }
}

// You must NOT do anything async signal unsafe in here!
bool WG14_SIGNALS_PREFIX(thrd_signal_raise)(int signo, void *raw_info,
                                            void *raw_context)
{
  if(0 != thrd_signal_global_tss_state_init())
  {
    return false;
  }
  struct thrd_signal_global_state_tss_state_t *tss =
  thrd_signal_global_tss_state();
  struct thrd_signal_global_state_tss_state_per_frame_t *old = tss->front,
                                                        current;
  memset(&current, 0, sizeof(current));
  current.prev = old;
  tss->front = &current;
  if(setjmp(current.buf) != 0)
  {
    tss->front = old;
    return true;
  }

  const DWORD win32sehcode = win32_exception_code_from_signal(signo);
  EXCEPTION_RECORD *info = (EXCEPTION_RECORD *) raw_info;
  // info->ExceptionInformation[0] = 0=read 1=write 8=DEP
  // info->ExceptionInformation[1] = causing address
  // info->ExceptionInformation[2] = NTSTATUS causing exception
  if(info != WG14_SIGNALS_NULLPTR)
  {
    if(raw_context != WG14_SIGNALS_NULLPTR &&
       info->NumberParameters < EXCEPTION_MAXIMUM_PARAMETERS - 2)
    {
      info->ExceptionInformation[info->NumberParameters++] =
      (ULONG_PTR) 0xdeadbeefdeadbeef;
      info->ExceptionInformation[info->NumberParameters++] =
      (ULONG_PTR) raw_context;
    }
    RaiseException(win32sehcode, info->ExceptionFlags, info->NumberParameters,
                   info->ExceptionInformation);
  }
  else
  {
    RaiseException(win32sehcode, 0, 0, WG14_SIGNALS_NULLPTR);
  }
  return true;
}

static long __stdcall win32_vectored_exception_function(
EXCEPTION_POINTERS *ptrs)
{
  const int signo =
  signal_from_win32_exception_code(ptrs->ExceptionRecord->ExceptionCode);
  if(signo == 0)
  {
    // Not a supported exception code
    return EXCEPTION_CONTINUE_SEARCH;
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
    return EXCEPTION_CONTINUE_SEARCH;
  }
  struct WG14_SIGNALS_PREFIX(thrd_raised_signal_info) rsi;
  prepare_rsi(&rsi, signo, ptrs);
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
        struct thrd_signal_global_state_tss_state_t *tss =
        thrd_signal_global_tss_state();
        // If there is a most recent thread local handler, resume there instead
        if(tss->front != WG14_SIGNALS_NULLPTR)
        {
          longjmp(tss->front->buf, 1);
        }
        // This will generally end the process
        return EXCEPTION_CONTINUE_EXECUTION;
      }
      LOCK(state->lock);
      current = current->next;
    } while(current != WG14_SIGNALS_NULLPTR);
  }
  // None of our deciders want this, so call previously installed signal handler
  UNLOCK(state->lock);
  return EXCEPTION_CONTINUE_SEARCH;
}

/* The interaction between AddVectoredContinueHandler,
AddVectoredExceptionHandler, UnhandledExceptionFilter, and frame-based EH is
completely undocumented in Microsoft documentation. The following is the truth,
as determined by empirical testing:

1. Vectored exception handlers get called first, before anything else, including
frame-based EH. This is not what the MSDN documentation hints at.

2. Frame-based EH filters are now run.

3. UnhandledExceptionFilter() is now called. On older Windows, this invokes the
debugger if being run under the debugger, otherwise continues search. But as of
at least Windows 7 onwards, if no debugger is attached, it invokes Windows
Error Reporting to send a core dump to Microsoft.

4. Vectored continue handlers now get called, AFTER the frame-based EH. Again,
not what MSDN hints at.


The change in the default non-debugger behaviour of UnhandledExceptionFilter()
effectively makes vectored continue handlers useless. I suspect whomever made
the change at Microsoft didn't realise that vectored continue handlers are
invoked AFTER the unhandled exception filter, because that's really non-obvious
from the documentation.

Anyway this is why we install for both the continue handler and the unhandled
exception filters. The unhandled exception filter will be called when not
running under a debugger. The vectored continue handler will be called when
running under a debugger, as the UnhandledExceptionFilter() function never calls
the installed unhandled exception filter function if under a debugger.
*/

static bool install_sighandler_impl(struct sighandler_t *item, const int signo)
{
  (void) item;
  (void) signo;
  struct WG14_SIGNALS_PREFIX(thrd_signal_global_state_t) *state =
  WG14_SIGNALS_PREFIX(thrd_signal_global_state)();
  if(0 == state->sighandlers_count)
  {
    state->vectored_continue_handler =
    AddVectoredContinueHandler(true, win32_vectored_exception_function);
    if(state->vectored_continue_handler == WG14_SIGNALS_NULLPTR)
    {
      return false;
    }
    state->old_unhandled_exception_filter =
    SetUnhandledExceptionFilter(win32_vectored_exception_function);
  }
  return true;
}
static bool uninstall_sighandler_impl(struct sighandler_t *item,
                                      const int signo)
{
  (void) item;
  (void) signo;
  struct WG14_SIGNALS_PREFIX(thrd_signal_global_state_t) *state =
  WG14_SIGNALS_PREFIX(thrd_signal_global_state)();
  if(0 == state->sighandlers_count)
  {
    SetUnhandledExceptionFilter(state->old_unhandled_exception_filter);
    RemoveVectoredContinueHandler(state->vectored_continue_handler);
  }
  return true;
}
