use std::panic;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;

fn main() {
    // hook counting + catch_unwind
    let count = Arc::new(AtomicUsize::new(0));
    let count2 = Arc::clone(&count);
    panic::set_hook(Box::new(move |info| {
        count2.fetch_add(1, Ordering::SeqCst);
        println!("hook: {}", info);
    }));

    let result = panic::catch_unwind(|| panic!("counted panic"));
    println!("caught = {}", result.is_err());

    let ok = panic::catch_unwind(|| 5 + 5);
    println!("no panic = {}", ok.unwrap());

    panic::take_hook();
    println!("hook count = {}", count.load(Ordering::SeqCst));

    // panic_any with a payload + downcast
    let payload = panic::catch_unwind(|| panic::panic_any(String::from("payload string")));
    let msg = payload
        .err()
        .and_then(|p| p.downcast_ref::<String>().cloned());
    println!("payload = {:?}", msg);

    // &str payload
    let str_payload = panic::catch_unwind(|| panic!("static str"));
    let s = str_payload.err().and_then(|p| p.downcast_ref::<&str>().map(|s| *s));
    println!("str payload = {:?}", s);

    // no hook: the default hook prints; suppress by setting an empty hook
    panic::set_hook(Box::new(|_| {}));
    let silent = panic::catch_unwind(|| panic!("silent panic"));
    println!("silent caught = {}", silent.is_err());
    panic::take_hook();

    // resume_unwind style: catch and inspect a structured payload
    #[derive(Debug)]
    struct Boom { code: u32 }
    let structured = panic::catch_unwind(|| panic::panic_any(Boom { code: 77 }));
    let code = structured.err().and_then(|p| p.downcast_ref::<Boom>().map(|b| b.code));
    println!("structured = {:?}", code);

    // panic in a thread -> join Err
    let h = std::thread::spawn(|| panic!("thread panic"));
    let joined = h.join();
    println!("thread panic caught = {}", joined.is_err());
}
