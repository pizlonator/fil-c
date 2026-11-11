use std::sync::{Arc, Barrier, LazyLock, Mutex, Once, OnceLock};
use std::thread;

fn main() {
    // Once racing
    let once = Arc::new(Once::new());
    let count = Arc::new(Mutex::new(0u32));
    let barrier = Arc::new(Barrier::new(4));
    let mut handles = Vec::new();
    for _ in 0..4 {
        let (o, c, b) = (Arc::clone(&once), Arc::clone(&count), Arc::clone(&barrier));
        handles.push(thread::spawn(move || {
            b.wait();
            o.call_once(|| {
                *c.lock().unwrap() += 1;
            });
        }));
    }
    for h in handles { h.join().unwrap(); }
    println!("once ran = {}", *count.lock().unwrap());

    // OnceLock racing
    let lock = Arc::new(OnceLock::<String>::new());
    let mut h2 = Vec::new();
    for i in 0..4 {
        let l = Arc::clone(&lock);
        h2.push(thread::spawn(move || {
            l.get_or_init(|| format!("value-{}", i)).clone()
        }));
    }
    let mut vals: Vec<String> = h2.into_iter().map(|h| h.join().unwrap()).collect();
    vals.sort();
    println!("oncelock distinct = {}", vals.iter().collect::<std::collections::BTreeSet<_>>().len());
    println!("oncelock all same = {}", vals[0] == vals[3]);
    println!("oncelock get = {}", lock.get().unwrap());

    // LazyLock
    let lazy = LazyLock::<Vec<Box<String>>>::new(|| {
        vec![Box::new("lazy-a".to_string()), Box::new("lazy-b".to_string())]
    });
    println!("lazy = {} {}", lazy[0], lazy[1]);

    // OnceLock inline
    let cell: OnceLock<u64> = OnceLock::new();
    println!("empty = {}", cell.get().is_none());
    assert!(cell.set(42).is_ok());
    println!("set = {} again = {:?}", cell.get().unwrap(), cell.set(43).is_err());
}
