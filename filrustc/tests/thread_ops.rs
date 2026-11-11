use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{mpsc, Arc};
use std::thread::{self, Thread};
use std::time::Duration;

fn main() {
    // park/unpark
    let parked = thread::current();
    let flag = Arc::new(AtomicBool::new(false));
    let flag2 = Arc::clone(&flag);
    let unparker = thread::spawn(move || {
        thread::sleep(Duration::from_millis(20));
        flag2.store(true, Ordering::SeqCst);
        parked.unpark();
    });
    while !flag.load(Ordering::SeqCst) {
        thread::park();
    }
    unparker.join().unwrap();
    println!("parked/unparked ok");

    // yield_now + tiny sleeps
    let (tx, rx) = mpsc::channel();
    let yielder = thread::spawn(move || {
        for i in 0..5 {
            thread::yield_now();
            thread::sleep(Duration::from_millis(1));
            tx.send(i).unwrap();
        }
    });
    let collected: Vec<i32> = rx.iter().take(5).collect();
    yielder.join().unwrap();
    println!("yielded = {:?}", collected);

    // available_parallelism
    let par = thread::available_parallelism().map(|n| n.get()).unwrap_or(0);
    println!("parallelism = {}", par >= 1);

    // named thread + builder + is_finished
    let handle = thread::Builder::new().name("worker".to_string()).stack_size(256 * 1024).spawn(|| {
        let me: Thread = thread::current();
        println!("named = {}", me.name().unwrap_or("?"));
        thread::sleep(Duration::from_millis(10));
        77
    }).unwrap();
    println!("is_finished before = {}", handle.is_finished());
    let result = handle.join().unwrap();
    println!("joined = {}", result);

    // scoped threads with borrows
    let data: Vec<Box<String>> = vec![Box::new("s0".to_string()), Box::new("s1".to_string())];
    let refs: Vec<&str> = thread::scope(|s| {
        let h1 = s.spawn(|| data.iter().map(|b| b.as_str()).collect::<Vec<_>>().join("+"));
        let h2 = s.spawn(|| data.len());
        let a = h1.join().unwrap();
        let b = h2.join().unwrap();
        println!("scoped = {} len = {}", a, b);
        data.iter().map(|b| b.as_str()).collect::<Vec<_>>()
    });
    println!("after scope = {}", refs.join(","));

    // thread::scope with a panic observed
    let caught = thread::scope(|s| {
        let h = s.spawn(|| panic!("scoped boom"));
        h.join().is_err()
    });
    println!("scoped panic caught = {}", caught);

    // many short threads
    let mut handles = Vec::new();
    for i in 0..8 {
        handles.push(thread::spawn(move || i * i));
    }
    let squares: Vec<i32> = handles.into_iter().map(|h| h.join().unwrap()).collect();
    println!("squares = {:?}", squares);
}
