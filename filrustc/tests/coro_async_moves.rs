// Coroutines: async fns whose futures move large pointer-containing captures
// across await points, polled by a hand-rolled executor.  Needs edition 2021
// (manifest extraArgs) and deterministic output.
use std::future::Future;
use std::pin::Pin;
use std::task::{Context, Poll, RawWaker, RawWakerVTable, Waker};

fn noop_waker() -> Waker {
    fn clone(_: *const ()) -> RawWaker { RawWaker::new(std::ptr::null(), &VTABLE) }
    fn wake(_: *const ()) {}
    fn wake_by_ref(_: *const ()) {}
    fn drop(_: *const ()) {}
    static VTABLE: RawWakerVTable = RawWakerVTable::new(clone, wake, wake_by_ref, drop);
    // SAFETY: the vtable functions never dereference the data pointer.
    unsafe { Waker::from_raw(RawWaker::new(std::ptr::null(), &VTABLE)) }
}

#[inline(never)]
async fn work(boxed: Box<String>, list: Box<Vec<String>>) -> String {
    let mut acc = (*boxed).clone();
    acc.push('+');
    for item in list.iter() {
        acc.push_str(item);
    }
    let bracketed = format!("[{}]", acc);
    let _noop: () = std::future::ready(()).await;
    let owned = bracketed.clone();
    drop(bracketed);
    owned + "!"
}

async fn chain(first: Box<String>) -> String {
    let step1 = work(first, Box::new(vec!["a".to_string(), "b".to_string()])).await;
    let step2 = work(Box::new(step1), Box::new(vec!["c".to_string()])).await;
    step2
}

fn main() {
    let fut = chain(Box::new("seed".to_string()));
    let mut fut = Box::pin(fut);
    let waker = noop_waker();
    let mut cx = Context::from_waker(&waker);
    match Pin::new(&mut fut).as_mut().poll(&mut cx) {
        Poll::Ready(s) => println!("done = {}", s),
        Poll::Pending => println!("pending?!"),
    }
}
