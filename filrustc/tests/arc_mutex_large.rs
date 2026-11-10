// Threads mutating shared Arc<Mutex<Vec<String>>> with large captured
// structs; deterministic because the final print sorts.
use std::sync::{Arc, Mutex};
use std::thread;

#[inline(never)]
fn big_capture(id: u32) -> (String, Vec<Box<String>>) {
    (
        format!("capture-{}", id),
        (0..5).map(|i| Box::new(format!("c{}-{}", id, i))).collect(),
    )
}

fn main() {
    let shared: Arc<Mutex<Vec<String>>> = Arc::new(Mutex::new(Vec::new()));
    let mut handles = Vec::new();
    for i in 0..4u32 {
        let shared = Arc::clone(&shared);
        let (name, data) = big_capture(i);
        handles.push(thread::spawn(move || {
            let mut collected: Vec<String> = Vec::new();
            collected.push(name);
            for d in &data {
                collected.push((**d).clone());
            }
            let mut guard = shared.lock().unwrap();
            guard.extend(collected);
        }));
    }
    for h in handles {
        h.join().unwrap();
    }
    let guard = shared.lock().unwrap();
    let mut sorted = guard.clone();
    sorted.sort();
    println!("n = {} all = {}", guard.len(), sorted.join(","));
}
