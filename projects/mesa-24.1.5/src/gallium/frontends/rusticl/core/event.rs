use crate::api::icd::*;
use crate::api::types::*;
use crate::core::context::*;
use crate::core::queue::*;
use crate::impl_cl_type_trait;

use mesa_rust::pipe::query::*;
use mesa_rust_gen::*;
use mesa_rust_util::static_assert;
use rusticl_opencl_gen::*;

use std::collections::HashSet;
use std::mem;
use std::sync::Arc;
use std::sync::Condvar;
use std::sync::Mutex;
use std::sync::MutexGuard;
use std::time::Duration;

// we assert that those are a continous range of numbers so we won't have to use HashMaps
static_assert!(CL_COMPLETE == 0);
static_assert!(CL_RUNNING == 1);
static_assert!(CL_SUBMITTED == 2);
static_assert!(CL_QUEUED == 3);

pub type EventSig = Box<dyn FnOnce(&Arc<Queue>, &QueueContext) -> CLResult<()> + Send + Sync>;

pub enum EventTimes {
    Queued = CL_PROFILING_COMMAND_QUEUED as isize,
    Submit = CL_PROFILING_COMMAND_SUBMIT as isize,
    Start = CL_PROFILING_COMMAND_START as isize,
    End = CL_PROFILING_COMMAND_END as isize,
}

#[derive(Default)]
struct EventMutState {
    status: cl_int,
    cbs: [Vec<EventCB>; 3],
    work: Option<EventSig>,
    time_queued: cl_ulong,
    time_submit: cl_ulong,
    time_start: cl_ulong,
    time_end: cl_ulong,
}

pub struct Event {
    pub base: CLObjectBase<CL_INVALID_EVENT>,
    pub context: Arc<Context>,
    pub queue: Option<Arc<Queue>>,
    pub cmd_type: cl_command_type,
    pub deps: Vec<Arc<Event>>,
    state: Mutex<EventMutState>,
    cv: Condvar,
}

impl_cl_type_trait!(cl_event, Event, CL_INVALID_EVENT);

impl Event {
    pub fn new(
        queue: &Arc<Queue>,
        cmd_type: cl_command_type,
        deps: Vec<Arc<Event>>,
        work: EventSig,
    ) -> Arc<Event> {
        Arc::new(Self {
            base: CLObjectBase::new(RusticlTypes::Event),
            context: queue.context.clone(),
            queue: Some(queue.clone()),
            cmd_type: cmd_type,
            deps: deps,
            state: Mutex::new(EventMutState {
                status: CL_QUEUED as cl_int,
                work: Some(work),
                ..Default::default()
            }),
            cv: Condvar::new(),
        })
    }

    pub fn new_user(context: Arc<Context>) -> Arc<Event> {
        Arc::new(Self {
            base: CLObjectBase::new(RusticlTypes::Event),
            context: context,
            queue: None,
            cmd_type: CL_COMMAND_USER,
            deps: Vec::new(),
            state: Mutex::new(EventMutState {
                status: CL_SUBMITTED as cl_int,
                ..Default::default()
            }),
            cv: Condvar::new(),
        })
    }

    fn state(&self) -> MutexGuard<EventMutState> {
        self.state.lock().unwrap()
    }

    pub fn status(&self) -> cl_int {
        self.state().status
    }

    fn set_status(&self, mut lock: MutexGuard<EventMutState>, new: cl_int) {
        lock.status = new;

        // signal on completion or an error
        if new <= CL_COMPLETE as cl_int {
            self.cv.notify_all();
        }

        // errors we treat as CL_COMPLETE
        let cb_max = if new < 0 { CL_COMPLETE } else { new as u32 };

        // there are only callbacks for those
        if ![CL_COMPLETE, CL_RUNNING, CL_SUBMITTED].contains(&cb_max) {
            return;
        }

        let mut cbs = Vec::new();
        // Collect all cbs we need to call first. Technically it is not required to call them in
        // order, but let's be nice to applications as it's for free
        for idx in (cb_max..=CL_SUBMITTED).rev() {
            cbs.extend(
                // use mem::take as each callback is only supposed to be called exactly once
                mem::take(&mut lock.cbs[idx as usize])
                    .into_iter()
                    // we need to save the status this cb was registered with
                    .map(|cb| (idx as cl_int, cb)),
            );
        }

        // applications might want to access the event in the callback, so drop the lock before
        // calling into the callbacks.
        drop(lock);

        for (idx, cb) in cbs {
            // from the CL spec:
            //
            // event_command_status is equal to the command_exec_callback_type used while
            // registering the callback. [...] If the callback is called as the result of the
            // command associated with event being abnormally terminated, an appropriate error code
            // for the error that caused the termination will be passed to event_command_status
            // instead.
            let status = if new < 0 { new } else { idx };
            cb.call(self, status);
        }
    }

    pub fn set_user_status(&self, status: cl_int) {
        self.set_status(self.state(), status);
    }

    pub fn is_error(&self) -> bool {
        self.status() < 0
    }

    pub fn is_user(&self) -> bool {
        self.cmd_type == CL_COMMAND_USER
    }

    pub fn set_time(&self, which: EventTimes, value: cl_ulong) {
        let mut lock = self.state();
        match which {
            EventTimes::Queued => lock.time_queued = value,
            EventTimes::Submit => lock.time_submit = value,
            EventTimes::Start => lock.time_start = value,
            EventTimes::End => lock.time_end = value,
        }
    }

    pub fn get_time(&self, which: EventTimes) -> cl_ulong {
        let lock = self.state();

        match which {
            EventTimes::Queued => lock.time_queued,
            EventTimes::Submit => lock.time_submit,
            EventTimes::Start => lock.time_start,
            EventTimes::End => lock.time_end,
        }
    }

    pub fn add_cb(&self, state: cl_int, cb: EventCB) {
        let mut lock = self.state();
        let status = lock.status;

        // call cb if the status was already reached
        if state >= status {
            drop(lock);
            cb.call(self, state);
        } else {
            lock.cbs.get_mut(state as usize).unwrap().push(cb);
        }
    }

    pub(super) fn signal(&self) {
        let state = self.state();
        // we don't want to call signal on errored events, but if that still happens, handle it
        // gracefully
        debug_assert_eq!(state.status, CL_SUBMITTED as cl_int);
        if state.status < 0 {
            return;
        }
        self.set_status(state, CL_RUNNING as cl_int);
        self.set_status(self.state(), CL_COMPLETE as cl_int);
    }

    pub fn wait(&self) -> cl_int {
        let mut lock = self.state();
        while lock.status >= CL_RUNNING as cl_int {
            lock = self
                .cv
                .wait_timeout(lock, Duration::from_secs(1))
                .unwrap()
                .0;
        }
        lock.status
    }

    // We always assume that work here simply submits stuff to the hardware even if it's just doing
    // sw emulation or nothing at all.
    // If anything requets waiting, we will update the status through fencing later.
    pub fn call(&self, ctx: &QueueContext) -> cl_int {
        let mut lock = self.state();
        let mut status = lock.status;
        let queue = self.queue.as_ref().unwrap();
        let profiling_enabled = queue.is_profiling_enabled();
        if status == CL_QUEUED as cl_int {
            if profiling_enabled {
                // We already have the lock so can't call set_time on the event
                lock.time_submit = queue.device.screen().get_timestamp();
            }
            let mut query_start = None;
            let mut query_end = None;
            status = lock.work.take().map_or(
                // if there is no work
                CL_SUBMITTED as cl_int,
                |w| {
                    if profiling_enabled {
                        query_start =
                            PipeQueryGen::<{ pipe_query_type::PIPE_QUERY_TIMESTAMP }>::new(ctx);
                    }

                    let res = w(queue, ctx).err().map_or(
                        // if there is an error, negate it
                        CL_SUBMITTED as cl_int,
                        |e| e,
                    );
                    if profiling_enabled {
                        query_end =
                            PipeQueryGen::<{ pipe_query_type::PIPE_QUERY_TIMESTAMP }>::new(ctx);
                    }
                    res
                },
            );

            if profiling_enabled {
                lock.time_start = query_start.unwrap().read_blocked();
                lock.time_end = query_end.unwrap().read_blocked();
            }
            self.set_status(lock, status);
        }
        status
    }

    fn deep_unflushed_deps_impl<'a>(&'a self, result: &mut HashSet<&'a Event>) {
        if self.status() <= CL_SUBMITTED as i32 {
            return;
        }

        // only scan dependencies if it's a new one
        if result.insert(self) {
            for e in &self.deps {
                e.deep_unflushed_deps_impl(result);
            }
        }
    }

    /// does a deep search and returns a list of all dependencies including `events` which haven't
    /// been flushed out yet
    pub fn deep_unflushed_deps(events: &[Arc<Event>]) -> HashSet<&Event> {
        let mut result = HashSet::new();

        for e in events {
            e.deep_unflushed_deps_impl(&mut result);
        }

        result
    }

    /// does a deep search and returns a list of all queues which haven't been flushed yet
    pub fn deep_unflushed_queues(events: &[Arc<Event>]) -> HashSet<Arc<Queue>> {
        Event::deep_unflushed_deps(events)
            .iter()
            .filter_map(|e| e.queue.clone())
            .collect()
    }
}

impl Drop for Event {
    // implement drop in order to prevent stack overflows of long dependency chains.
    //
    // This abuses the fact that `Arc::into_inner` only succeeds when there is one strong reference
    // so we turn a recursive drop chain into a drop list for events having no other references.
    fn drop(&mut self) {
        if self.deps.is_empty() {
            return;
        }

        let mut deps_list = vec![mem::take(&mut self.deps)];
        while let Some(deps) = deps_list.pop() {
            for dep in deps {
                if let Some(mut dep) = Arc::into_inner(dep) {
                    deps_list.push(mem::take(&mut dep.deps));
                }
            }
        }
    }
}

// TODO worker thread per device
// Condvar to wait on new events to work on
// notify condvar when flushing queue events to worker
// attach fence to flushed events on context->flush
// store "newest" event for in-order queues per queue
// reordering/graph building done in worker
