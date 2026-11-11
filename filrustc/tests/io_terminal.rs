// `io::IsTerminal`: the isatty-style queries (std::io::stdin/stdout/stderr
// and `File::is_terminal` through the generic trait surface).  The harness
// always runs the test with its output redirected, so every query here is
// deterministically false -- and the trait works (a wrong syscall path or a
// broken trait impl would panic or report true).
use std::fs::File;
use std::io::{IsTerminal, Stderr, Stdin, Stdout};

// The trait surface: IsTerminal is implemented for the std streams and for
// File; a generic helper proves the trait method is usable generically.
fn report<T: IsTerminal>(what: &str, t: &T) -> bool {
    let is_tty = t.is_terminal();
    println!("{} is_tty = {}", what, is_tty);
    is_tty
}

fn main() {
    let stdin: Stdin = std::io::stdin();
    let stdout: Stdout = std::io::stdout();
    let stderr: Stderr = std::io::stderr();
    let s = report("stdin", &stdin);
    let o = report("stdout", &stdout);
    let e = report("stderr", &stderr);

    // A real file is never a terminal.
    let dir = std::env::temp_dir().join(format!("filc-isterm-{}", std::process::id()));
    std::fs::create_dir(&dir).unwrap();
    let f = File::create(dir.join("f.txt")).unwrap();
    let file_tty = report("file", &f);
    drop(f);

    println!("all not terminals = {}", !(s || o || e || file_tty));

    std::fs::remove_dir_all(&dir).unwrap();
    println!("cleaned = true");
}
