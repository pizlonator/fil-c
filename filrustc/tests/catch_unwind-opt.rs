use std::panic;

fn main() {
    // A panicking closure is caught and its payload can be inspected.
    let result = panic::catch_unwind(|| {
        panic!("boom");
    });
    match result {
        Err(payload) => {
            let msg = payload
                .downcast_ref::<&str>()
                .copied()
                .or_else(|| payload.downcast_ref::<String>().map(|s| s.as_str()))
                .unwrap_or("<unknown payload>");
            println!("caught: {}", msg);
        }
        Ok(()) => {
            println!("a panic! was not caught?!");
            std::process::exit(1);
        }
    }

    // A non-panicking closure comes back as Ok.
    let ok = panic::catch_unwind(|| 6 * 7);
    match ok {
        Ok(answer) if answer == 42 => println!("ok path"),
        _ => {
            println!("a non-panicking closure did not come back Ok");
            std::process::exit(1);
        }
    }
}
