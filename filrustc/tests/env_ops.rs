use std::env;
use std::path::Path;

fn main() {
    // var/set_var/remove_var (single-threaded section)
    env::set_var("FILC_TEST_ENV", "the-value");
    println!("var = {}", env::var("FILC_TEST_ENV").unwrap());
    println!("var_os = {}", env::var_os("FILC_TEST_ENV").unwrap().to_string_lossy());
    env::set_var("FILC_TEST_ENV", "changed");
    println!("changed = {}", env::var("FILC_TEST_ENV").unwrap());
    env::remove_var("FILC_TEST_ENV");
    println!("removed = {}", env::var("FILC_TEST_ENV").is_err());
    println!("missing = {}", env::var("FILC_DEFINITELY_NOT_SET_XYZ").is_err());
    println!("home-ish = {}", env::var("PATH").is_ok());

    // current_dir + temp_dir
    let cwd = env::current_dir().unwrap();
    println!("current_dir exists = {}", Path::new(&cwd).exists());
    let tmp = env::temp_dir();
    println!("temp_dir = {}", tmp.to_string_lossy().contains("tmp"));

    // args (the test binary path)
    let args: Vec<String> = env::args().collect();
    println!("args has self = {}", !args.is_empty() && args[0].len() > 0);

    // split_paths / join_paths
    let joined = env::join_paths(["/a", "/b"]).unwrap();
    println!("join_paths len = {}", joined.len() > 0);
    let split: Vec<_> = env::split_paths(&joined).collect();
    println!("split_paths = {}", split.len() == 2);
}
