// Panics carrying LARGE pointer-bearing struct payloads: panic_any +
// catch_unwind + Box<dyn Any> downcast.  The payload moves through the
// panic machinery as a boxed fat pointer; every word must survive.
use std::panic;

#[inline(never)]
fn make_payload(id: u32) -> Payload {
    Payload {
        name: format!("payload-{}", id),
        boxed: Box::new(id as u64 * 7),
        list: vec![format!("l{}-a", id), format!("l{}-b", id)],
    }
}

struct Payload {
    name: String,
    boxed: Box<u64>,
    list: Vec<String>,
}

impl std::fmt::Display for Payload {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}#{}:{}", self.name, self.boxed, self.list.join("+"))
    }
}

fn main() {
    let payload = make_payload(11);
    let caught = panic::catch_unwind(move || {
        let inner = make_payload(12);
        panic::panic_any(inner);
    });
    let restored = caught.unwrap_err();
    let down = restored.downcast::<Payload>().unwrap();
    println!("down = {}", down);
    println!("orig = {}", payload);

    // Box<dyn Any> downcast_ref through a panic! with a non-const formatted
    // payload (a literal-only panic! carries a &'static str payload instead)
    let caught2 = panic::catch_unwind(|| panic!("plain {}", "message".to_string()));
    let msg = caught2.unwrap_err().downcast_ref::<String>().unwrap().clone();
    println!("msg = {}", msg);
    let caught4 = panic::catch_unwind(|| panic!("static panic text"));
    let static_msg = caught4.unwrap_err().downcast_ref::<&'static str>().unwrap().to_string();
    println!("static = {}", static_msg);

    // panic_any of a struct wrapped in Box, downcast to the same type
    let caught3 = panic::catch_unwind(|| {
        panic::panic_any(make_payload(13));
    });
    let down3 = caught3.unwrap_err().downcast::<Payload>().unwrap();
    println!("down3 = {}", down3);
}
