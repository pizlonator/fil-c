use std::io::Write;
use std::process::{Command, Stdio};

fn main() {
    println!("pid nonzero = {}", std::process::id() > 0);

    let out = Command::new("/bin/echo").arg("hello from child").output().unwrap();
    println!("status = {} stdout = {}", out.status.success(), String::from_utf8_lossy(&out.stdout).trim());

    let st = Command::new("/bin/true").status().unwrap();
    println!("true status = {}", st.success());
    let st2 = Command::new("/bin/false").status().unwrap();
    println!("false status = {} code = {:?}", st2.success(), st2.code());

    let err = Command::new("/nonexistent-program-xyz").output().unwrap_err();
    println!("missing program = {}", err.kind() == std::io::ErrorKind::NotFound);

    let mut child = Command::new("/bin/cat")
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .spawn()
        .unwrap();
    {
        let stdin = child.stdin.as_mut().unwrap();
        stdin.write_all(b"piped data\n").unwrap();
    }
    drop(child.stdin.take());
    let out2 = child.wait_with_output().unwrap();
    println!("cat = {}", String::from_utf8_lossy(&out2.stdout).trim());

    let out3 = Command::new("/bin/echo")
        .env("FILC_TEST_VAR", "envvalue")
        .output()
        .unwrap();
    println!("env child ok = {}", out3.status.success());

    let out4 = Command::new("/bin/sh").arg("-c").arg("echo A; echo B >&2").output().unwrap();
    println!("stdout = {} stderr = {}",
        String::from_utf8_lossy(&out4.stdout).trim(),
        String::from_utf8_lossy(&out4.stderr).trim());

    let out5 = Command::new("/bin/sh").arg("-c").arg("exit 3").output().unwrap();
    println!("exit code = {:?}", out5.status.code());

    let args = Command::new("/bin/echo").args(["x", "y", "z"]).output().unwrap();
    println!("args = {}", String::from_utf8_lossy(&args.stdout).trim());

    let cwd = std::env::current_dir().unwrap();
    let out6 = Command::new("/bin/sh").arg("-c").arg("pwd").current_dir(&cwd).output().unwrap();
    println!("pwd basename = {}", String::from_utf8_lossy(&out6.stdout).trim().rsplit('/').next().unwrap().is_empty() == false);
}
