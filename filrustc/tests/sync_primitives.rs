use std::sync::{Arc, Barrier, Condvar, Mutex, RwLock};
use std::thread;

fn main() {
    // Mutex poison + into_inner
    let m = Arc::new(Mutex::new(0u64));
    let m2 = Arc::clone(&m);
    let h = thread::spawn(move || {
        let _g = m2.lock().unwrap();
        panic!("poison the mutex");
    });
    let _ = h.join();
    let poisoned = m.lock().is_err();
    println!("poisoned = {}", poisoned);
    let data = Arc::try_unwrap(m).unwrap().into_inner().unwrap_or(0);
    println!("into_inner = {}", data);

    // healthy mutex across threads
    let counter = Arc::new(Mutex::new(Vec::<String>::new()));
    let mut handles = Vec::new();
    for i in 0..4 {
        let c = Arc::clone(&counter);
        handles.push(thread::spawn(move || {
            c.lock().unwrap().push(format!("t{}", i));
        }));
    }
    for h in handles { h.join().unwrap(); }
    let mut items = counter.lock().unwrap().clone();
    items.sort();
    println!("counter = {}", items.join(","));

    // RwLock: multiple readers + exclusive writer
    let rw = Arc::new(RwLock::new(String::from("initial")));
    let r1 = rw.read().unwrap();
    let r2 = rw.read().unwrap();
    println!("readers = {} {}", r1.len(), r2.clone().len());
    drop((r1, r2));
    *rw.write().unwrap() = "written".to_string();
    println!("after write = {}", *rw.read().unwrap());
    let rw2 = Arc::clone(&rw);
    let hw = thread::spawn(move || {
        *rw2.write().unwrap() = "thread-write".to_string();
    });
    hw.join().unwrap();
    println!("thread write = {}", *rw.read().unwrap());

    // RwLock poison
    let rwp = Arc::new(RwLock::new(0u32));
    let rwp2 = Arc::clone(&rwp);
    let hp = thread::spawn(move || {
        let _g = rwp2.write().unwrap();
        panic!("poison the rwlock");
    });
    let _ = hp.join();
    println!("rw poisoned = {}", rwp.read().is_err());

    // Condvar wait/notify
    let pair = Arc::new((Mutex::new(false), Condvar::new()));
    let pair2 = Arc::clone(&pair);
    let hn = thread::spawn(move || {
        let (lock, cvar) = &*pair2;
        thread::sleep(std::time::Duration::from_millis(20));
        let mut ready = lock.lock().unwrap();
        *ready = true;
        cvar.notify_one();
    });
    let (lock, cvar) = &*pair;
    let mut ready = lock.lock().unwrap();
    while !*ready {
        ready = cvar.wait(ready).unwrap();
    }
    println!("condvar notified = {}", *ready);
    hn.join().unwrap();

    // wait_timeout
    let pair3 = Arc::new((Mutex::new(0u32), Condvar::new()));
    let (lock3, cvar3) = &*pair3;
    let l3 = lock3.lock().unwrap();
    let (l3, timeout) = cvar3.wait_timeout(l3, std::time::Duration::from_millis(30)).unwrap();
    println!("wait_timeout timed_out = {}", timeout.timed_out());
    drop(l3);

    // Barrier: 3 threads in 2 phases
    let barrier = Arc::new(Barrier::new(3));
    let mut bhandles = Vec::new();
    for i in 0..3 {
        let b = Arc::clone(&barrier);
        bhandles.push(thread::spawn(move || {
            let _w = b.wait();
            let _w2 = b.wait();
            i
        }));
    }
    let results: Vec<u32> = bhandles.into_iter().map(|h| h.join().unwrap()).collect();
    println!("barrier = {:?}", results);
}
