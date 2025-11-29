use crate::api::icd::*;
use crate::core::context::*;
use crate::core::device::*;
use crate::core::event::*;
use crate::core::platform::*;
use crate::impl_cl_type_trait;

use mesa_rust::pipe::context::PipeContext;
use mesa_rust_util::properties::*;
use rusticl_opencl_gen::*;

use std::cmp;
use std::mem;
use std::mem::ManuallyDrop;
use std::ops::Deref;
use std::sync::mpsc;
use std::sync::Arc;
use std::sync::Mutex;
use std::sync::Weak;
use std::thread;
use std::thread::JoinHandle;

/// State tracking wrapper for [PipeContext]
///
/// Used for tracking bound GPU state to lower CPU overhead and centralize state tracking
pub struct QueueContext {
    ctx: PipeContext,
    use_stream: bool,
}

impl QueueContext {
    fn new_for(device: &Device) -> CLResult<Self> {
        let ctx = device
            .screen()
            .create_context()
            .ok_or(CL_OUT_OF_HOST_MEMORY)?;

        Ok(Self {
            ctx: ctx,
            use_stream: device.prefers_real_buffer_in_cb0(),
        })
    }

    pub fn update_cb0(&self, data: &[u8]) -> CLResult<()> {
        // only update if we actually bind data
        if !data.is_empty() {
            if self.use_stream {
                if !self.ctx.set_constant_buffer_stream(0, data) {
                    return Err(CL_OUT_OF_RESOURCES);
                }
            } else {
                self.ctx.set_constant_buffer(0, data);
            }
        }
        Ok(())
    }
}

// This should go once we moved all state tracking into QueueContext
impl Deref for QueueContext {
    type Target = PipeContext;

    fn deref(&self) -> &Self::Target {
        &self.ctx
    }
}

impl Drop for QueueContext {
    fn drop(&mut self) {
        self.ctx.set_constant_buffer(0, &[])
    }
}

struct QueueState {
    pending: Vec<Arc<Event>>,
    last: Weak<Event>,
    // `Sync` on `Sender` was stabilized in 1.72, until then, put it into our Mutex.
    // see https://github.com/rust-lang/rust/commit/5f56956b3c7edb9801585850d1f41b0aeb1888ff
    chan_in: ManuallyDrop<mpsc::Sender<Vec<Arc<Event>>>>,
}

pub struct Queue {
    pub base: CLObjectBase<CL_INVALID_COMMAND_QUEUE>,
    pub context: Arc<Context>,
    pub device: &'static Device,
    pub props: cl_command_queue_properties,
    pub props_v2: Option<Properties<cl_queue_properties>>,
    state: Mutex<QueueState>,
    thrd: ManuallyDrop<JoinHandle<()>>,
}

impl_cl_type_trait!(cl_command_queue, Queue, CL_INVALID_COMMAND_QUEUE);

fn flush_events(evs: &mut Vec<Arc<Event>>, pipe: &PipeContext) {
    if !evs.is_empty() {
        pipe.flush().wait();
        evs.drain(..).for_each(|e| e.signal());
    }
}

impl Queue {
    pub fn new(
        context: Arc<Context>,
        device: &'static Device,
        props: cl_command_queue_properties,
        props_v2: Option<Properties<cl_queue_properties>>,
    ) -> CLResult<Arc<Queue>> {
        // we assume that memory allocation is the only possible failure. Any other failure reason
        // should be detected earlier (e.g.: checking for CAPs).
        let ctx = QueueContext::new_for(device)?;
        let (tx_q, rx_t) = mpsc::channel::<Vec<Arc<Event>>>();
        Ok(Arc::new(Self {
            base: CLObjectBase::new(RusticlTypes::Queue),
            context: context,
            device: device,
            props: props,
            props_v2: props_v2,
            state: Mutex::new(QueueState {
                pending: Vec::new(),
                last: Weak::new(),
                chan_in: ManuallyDrop::new(tx_q),
            }),
            thrd: ManuallyDrop::new(thread::Builder::new()
                .name("rusticl queue thread".into())
                .spawn(move || {
                    // Track the error of all executed events. This is only needed for in-order
                    // queues, so for out of order we'll need to update this.
                    // Also, the OpenCL specification gives us enough freedom to do whatever we want
                    // in case of any event running into an error while executing:
                    //
                    //   Unsuccessful completion results in abnormal termination of the command
                    //   which is indicated by setting the event status to a negative value. In this
                    //   case, the command-queue associated with the abnormally terminated command
                    //   and all other command-queues in the same context may no longer be available
                    //   and their behavior is implementation-defined.
                    //
                    // TODO: use pipe_context::set_device_reset_callback to get notified about gone
                    //       GPU contexts
                    let mut last_err = CL_SUCCESS as cl_int;
                    loop {
                    let r = rx_t.recv();
                    if r.is_err() {
                        break;
                    }

                    let new_events = r.unwrap();
                    let mut flushed = Vec::new();

                    for e in new_events {
                        // If we hit any deps from another queue, flush so we don't risk a dead
                        // lock.
                        if e.deps.iter().any(|ev| ev.queue != e.queue) {
                            flush_events(&mut flushed, &ctx);
                        }

                        // check if any dependency has an error
                        for dep in &e.deps {
                            // We have to wait on user events or events from other queues.
                            let dep_err = if dep.is_user() || dep.queue != e.queue {
                                dep.wait()
                            } else {
                                dep.status()
                            };

                            last_err = cmp::min(last_err, dep_err);
                        }

                        if last_err < 0 {
                            // If a dependency failed, fail this event as well.
                            e.set_user_status(last_err);
                            continue;
                        }

                        // if there is an execution error don't bother signaling it as the  context
                        // might be in a broken state. How queues behave after any event hit an
                        // error is entirely implementation defined.
                        last_err = e.call(&ctx);
                        if last_err < 0 {
                            continue;
                        }

                        if e.is_user() {
                            // On each user event we flush our events as application might
                            // wait on them before signaling user events.
                            flush_events(&mut flushed, &ctx);

                            // Wait on user events as they are synchronization points in the
                            // application's control.
                            e.wait();
                        } else if Platform::dbg().sync_every_event {
                            flushed.push(e);
                            flush_events(&mut flushed, &ctx);
                        } else {
                            flushed.push(e);
                        }
                    }

                    flush_events(&mut flushed, &ctx);
                    }
                })
                .unwrap()),
        }))
    }

    pub fn queue(&self, e: Arc<Event>) {
        if self.is_profiling_enabled() {
            e.set_time(EventTimes::Queued, self.device.screen().get_timestamp());
        }
        self.state.lock().unwrap().pending.push(e);
    }

    pub fn flush(&self, wait: bool) -> CLResult<()> {
        let mut state = self.state.lock().unwrap();
        let events = mem::take(&mut state.pending);
        let mut queues = Event::deep_unflushed_queues(&events);

        // Update last if and only if we get new events, this prevents breaking application code
        // doing things like `clFlush(q); clFinish(q);`
        if let Some(last) = events.last() {
            state.last = Arc::downgrade(last);

            // This should never ever error, but if it does return an error
            state
                .chan_in
                .send(events)
                .map_err(|_| CL_OUT_OF_HOST_MEMORY)?;
        }

        let last = wait.then(|| state.last.clone());

        // We have to unlock before actually flushing otherwise we'll run into dead locks when a
        // queue gets flushed concurrently.
        drop(state);

        // We need to flush out other queues implicitly and this _has_ to happen after taking the
        // pending events, otherwise we'll risk dead locks when waiting on events.
        queues.remove(self);
        for q in queues {
            q.flush(false)?;
        }

        if let Some(last) = last {
            // Waiting on the last event is good enough here as the queue will process it in order
            // It's not a problem if the weak ref is invalid as that means the work is already done
            // and waiting isn't necessary anymore.
            last.upgrade().map(|e| e.wait());
        }
        Ok(())
    }

    pub fn is_profiling_enabled(&self) -> bool {
        (self.props & (CL_QUEUE_PROFILING_ENABLE as u64)) != 0
    }
}

impl Drop for Queue {
    fn drop(&mut self) {
        // when deleting the application side object, we have to flush
        // From the OpenCL spec:
        // clReleaseCommandQueue performs an implicit flush to issue any previously queued OpenCL
        // commands in command_queue.
        // TODO: maybe we have to do it on every release?
        let _ = self.flush(true);

        let state = self.state.get_mut().unwrap();

        unsafe {
            // disconnect the channel
            ManuallyDrop::drop(&mut state.chan_in);

            // and now explicitly wait on the thread to quit, because it won't happen implicitly.
            ManuallyDrop::take(&mut self.thrd).join().unwrap();
        }
    }
}
