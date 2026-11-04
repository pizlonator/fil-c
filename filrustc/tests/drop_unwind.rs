use std::hint::black_box;
use std::panic;
use std::sync::{Arc, Mutex};

struct Guard {
    name: &'static str,
    log: Arc<Mutex<Vec<&'static str>>>,
}

impl Drop for Guard {
    fn drop(&mut self) {
        self.log.lock().unwrap().push(self.name);
    }
}

#[inline(never)]
fn deep(log: Arc<Mutex<Vec<&'static str>>>) {
    let _inner = Guard { name: "drop inner", log: Arc::clone(&log) };
    if black_box(true) {
        panic!("deep panic");
    }
}

#[inline(never)]
fn outer(log: Arc<Mutex<Vec<&'static str>>>) {
    let _outer = Guard { name: "drop outer", log: Arc::clone(&log) };
    deep(log);
}

fn main() {
    let log = Arc::new(Mutex::new(Vec::new()));
    let result = panic::catch_unwind(|| outer(black_box(Arc::clone(&log))));
    if result.is_ok() {
        println!("the panic was not caught?!");
        std::process::exit(1);
    }
    let events = log.lock().unwrap().clone();
    // Drop glue must run for every frame the panic unwinds through, and the
    // innermost frame's guard is dropped first.
    if events != vec!["drop inner", "drop outer"] {
        println!("unexpected drop order: {:?}", events);
        std::process::exit(1);
    }
    println!("drops: {}", events.join("; "));
    println!("drop ok");
}
