use std::sync::{Arc, Mutex};
use std::thread;

fn main() {
    let sum = Arc::new(Mutex::new(0u64));
    let mut handles = Vec::new();
    for _ in 0..8 {
        let sum = Arc::clone(&sum);
        handles.push(thread::spawn(move || {
            for _ in 0..1000 {
                let mut value = sum.lock().unwrap();
                *value += 1;
            }
        }));
    }
    for handle in handles {
        handle.join().unwrap();
    }
    println!("sum = {}", *sum.lock().unwrap());
}
