use std::io::prelude::*;
use std::os::unix::net::{UnixListener, UnixStream};
use std::net::{SocketAddr, ToSocketAddrs};
use std::thread;

fn main() {
    let dir = format!("/tmp/filc-unix-{}", std::process::id());
    std::fs::create_dir_all(&dir).unwrap();
    let sock_path = format!("{}/sock", dir);

    let listener = UnixListener::bind(&sock_path).unwrap();
    println!("bound local = {}", listener.local_addr().unwrap().as_pathname().is_some());

    let server = thread::spawn(move || {
        let (mut stream, addr) = listener.accept().unwrap();
        let mut buf = [0u8; 64];
        let n = stream.read(&mut buf).unwrap();
        stream.write_all(b"unix-echo:").unwrap();
        stream.write_all(&buf[..n]).unwrap();
        println!("peer addr is_pathname = {}", addr.as_pathname().is_some());
    });

    let mut client = UnixStream::connect(&sock_path).unwrap();
    client.write_all(b"ping").unwrap();
    let mut response = String::new();
    client.read_to_string(&mut response).unwrap();
    println!("unix echo = {}", response);
    server.join().unwrap();

    // pair
    let (mut s1, mut s2) = UnixStream::pair().unwrap();
    s1.write_all(b"pair!").unwrap();
    let mut got = [0u8; 5];
    s2.read_exact(&mut got).unwrap();
    println!("pair = {}", String::from_utf8(got.to_vec()).unwrap());

    // SocketAddr parsing and resolution
    let sa: SocketAddr = "127.0.0.1:8080".parse().unwrap();
    println!("parsed = {} {} v4 = {}", sa.ip(), sa.port(), sa.is_ipv4());
    let v6: SocketAddr = "[::1]:9".parse().unwrap();
    println!("v6 = {} loopback = {}", v6.ip().is_ipv6(), v6.ip().is_loopback());
    let resolved: Vec<_> = ("127.0.0.1", 12345).to_socket_addrs().unwrap().collect();
    println!("resolved = {}", resolved.len());
    let resolved2: Vec<_> = "127.0.0.1:9999".to_socket_addrs().unwrap().collect();
    println!("str resolved = {} first = {}", resolved2.len(), resolved2[0].port());
    let bad = "not-an-addr".parse::<SocketAddr>();
    println!("bad parse = {}", bad.is_err());

    std::fs::remove_file(&sock_path).unwrap();
    std::fs::remove_dir_all(&dir).unwrap();
    println!("cleaned = {}", !std::path::Path::new(&dir).exists());
}
