// thread_local! cells holding pointer-containing values, mutated from
// multiple threads and torn down with with_drop.
use std::cell::RefCell;
use std::thread;

thread_local! {
    static CELL: RefCell<Vec<String>> = RefCell::new(Vec::new());
    static WITH_DROP: RefCell<Box<String>> = RefCell::new(Box::new("dropped".to_string()));
}

fn main() {
    CELL.with(|c| c.borrow_mut().push("main".to_string()));
    CELL.with(|c| c.borrow_mut().push("main2".to_string()));

    let mut handles = Vec::new();
    for t in 0..2u32 {
        handles.push(thread::spawn(move || {
            WITH_DROP.with(|w| {
                *w.borrow_mut() = Box::new(format!("thread{}-value", t));
            });
            CELL.with(|c| {
                for i in 0..3 {
                    c.borrow_mut().push(format!("t{}-{}", t, i));
                }
                let joined = c.borrow().join(",");
                println!("thread{} sees: {}", t, joined);
            });
        }));
    }
    for h in handles {
        h.join().unwrap();
    }

    CELL.with(|c| println!("main sees: {}", c.borrow().join(",")));
    WITH_DROP.with(|w| println!("main drop cell: {}", w.borrow()));
}
