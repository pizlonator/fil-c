use std::thread;

fn main() {
    // A panicking thread is joined as Err, with the panic payload intact.
    let handle = thread::spawn(|| {
        panic!("thread boom");
    });
    let err = handle.join();
    match err {
        Err(payload) => {
            let msg = payload
                .downcast_ref::<&str>()
                .copied()
                .or_else(|| payload.downcast_ref::<String>().map(|s| s.as_str()))
                .unwrap_or("<unknown payload>");
            println!("thread panicked as expected: {}", msg);
        }
        Ok(()) => {
            println!("the thread panic was not observed?!");
            std::process::exit(1);
        }
    }

    // A healthy thread is joined as Ok.
    let healthy = thread::spawn(|| 6 * 7);
    match healthy.join() {
        Ok(answer) if answer == 42 => println!("healthy thread ok"),
        _ => {
            println!("the healthy thread did not come back Ok");
            std::process::exit(1);
        }
    }
}
