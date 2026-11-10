// Closures and boxed dyn: FnOnce boxed and called, Fn objects in a Vec,
// closures capturing large pointer-containing structs by value moved through
// iterator adapters, and move-closure state evolution.
use std::fmt::Write;

struct Captured {
    name: String,
    data: Vec<Box<String>>,
}

#[inline(never)]
fn make_captured(tag: &str) -> Captured {
    Captured {
        name: tag.to_string(),
        data: (0..4).map(|i| Box::new(format!("{}-d{}", tag, i))).collect(),
    }
}

fn main() {
    // FnOnce boxed and called
    let captured = make_captured("cap");
    let f: Box<dyn FnOnce() -> String> = Box::new(move || {
        let mut out = format!("{}:", captured.name);
        for d in &captured.data {
            write!(out, "{}", d).unwrap();
        }
        out
    });
    println!("fnonce = {}", f());

    // Box<dyn Fn> in a Vec, called repeatedly (state-free closures)
    let mut fns: Vec<Box<dyn Fn(u32) -> u32>> = Vec::new();
    fns.push(Box::new(|x| x + 1));
    fns.push(Box::new(|x| x * 2));
    fns.push(Box::new(|x| x - 3));
    let results: Vec<String> = fns.iter().map(|f| f(10).to_string()).collect();
    println!("fns = {}", results.join(","));

    // closure capturing a large struct by value, moved through map/filter/collect
    let c = make_captured("iter");
    let picked: Vec<String> = (0..4)
        .filter(|i| i % 2 == 0)
        .map(|i| format!("{}#{}", c.name, c.data[i].to_uppercase()))
        .collect();
    println!("picked = {}", picked.join(" "));

    // FnMut closure with mutable captured Vec, used twice; the closure owns
    // the captured struct, so all state reads go through it
    let mut pusher = {
        let mut owned = make_captured("mut");
        move |s: &str| -> String {
            owned.data.push(Box::new(s.to_string()));
            format!("len={} last={}", owned.data.len(), owned.data[owned.data.len() - 1])
        }
    };
    let r1 = pusher("x1");
    let r2 = pusher("x2");
    println!("pushed = {} {}", r1, r2);

    // boxed FnMut carrying its own counter (Rc-free, stateful)
    let counter = std::cell::RefCell::new(0u32);
    let counted = Box::new(move |x: u32| {
        *counter.borrow_mut() += x;
        *counter.borrow()
    });
    println!("counted = {} {}", counted(1), counted(2));
}
