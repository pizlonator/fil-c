// Once / OnceLock holding a large pointer-bearing value, initialized from
// competing threads exactly once; plus lazy maps keyed off it.
use std::sync::{Arc, Mutex, OnceLock};
use std::thread;

struct Config {
    names: Vec<String>,
    boxed: Box<u32>,
    nested: Vec<Vec<String>>,
}

impl Config {
    #[inline(never)]
    fn build() -> Config {
        Config {
            names: (0..6).map(|i| format!("cfg-name-{}", i)).collect(),
            boxed: Box::new(4242),
            nested: vec![vec!["n1".into(), "n2".into()], vec!["n3".into()]],
        }
    }
}

static CONFIG: OnceLock<Config> = OnceLock::new();

fn main() {
    let shared: Arc<Mutex<Vec<String>>> = Arc::new(Mutex::new(Vec::new()));
    let mut handles = Vec::new();
    for t in 0..4u32 {
        let shared = Arc::clone(&shared);
        handles.push(thread::spawn(move || {
            let cfg = CONFIG.get_or_init(Config::build);
            let mut got = format!("t{} {} {}", t, cfg.boxed, cfg.names[t as usize]);
            got.push_str(&format!(" nested={}", cfg.nested[0][1]));
            shared.lock().unwrap().push(got);
        }));
    }
    for h in handles {
        h.join().unwrap();
    }
    let mut lines = shared.lock().unwrap().clone();
    lines.sort();
    for l in &lines {
        println!("{}: {}", l.split(' ').next().unwrap(), l.splitn(2, ' ').nth(1).unwrap());
    }
    let cfg = CONFIG.get().unwrap();
    println!("final = {} {}", cfg.boxed, cfg.names.len());
}
