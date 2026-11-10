// std::thread::scope with large borrowed captures plus a worker that returns
// a big aggregate by value; deterministic via sorting at the end.
use std::thread;

#[inline(never)]
fn make_data(tag: &str, n: usize) -> Vec<Box<String>> {
    (0..n).map(|i| Box::new(format!("{}-{}", tag, i))).collect()
}

#[inline(never)]
fn summarize(data: &[Box<String>]) -> (usize, String) {
    (data.len(), data.iter().map(|b| b.as_str()).collect::<Vec<_>>().join(","))
}

fn main() {
    let shared = make_data("shared", 8);
    let mut results: Vec<String> = Vec::new();

    thread::scope(|s| {
        for t in 0..3u32 {
            let borrowed = &shared;
            let local = make_data(&format!("local{}", t), 4);
            results.push(s.spawn(move || {
                let (n, joined) = summarize(borrowed);
                let (ln, ljoined) = summarize(&local);
                format!("t{} shared{}:{} local{}:{}", t, n, joined.split(',').next().unwrap(), ln, ljoined)
            }).join().unwrap());
        }
        // a thread that panics and one that joins its Err
        let panicky = s.spawn(|| -> String {
            panic!("scoped boom");
        });
        let panic_result = panicky.join();
        println!("panic caught = {}", panic_result.is_err());
    });

    results.sort();
    for r in &results {
        println!("res = {}", r);
    }
    println!("shared intact = {}", shared[0].as_str() == "shared-0");
}
