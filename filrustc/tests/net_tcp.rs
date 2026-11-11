use std::io::{prelude::*, Read};
use std::net::{TcpListener, TcpStream};
use std::thread;
use std::time::Duration;

fn main() {
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let addr = listener.local_addr().unwrap();
    println!("bound = {}", addr.ip().is_loopback());
    let addr2 = listener.local_addr().unwrap();

    let server = thread::spawn(move || {
        let (mut stream, peer) = listener.accept().unwrap();
        println!("peer ip loopback = {}", peer.ip().is_loopback());
        let mut buf = [0u8; 64];
        let n = stream.read(&mut buf).unwrap();
        stream.write_all(&buf[..n]).unwrap();
        stream.flush().unwrap();
        n
    });

    let mut client = TcpStream::connect(addr).unwrap();
    println!("client local loopback = {} nodelay = {}", client.local_addr().unwrap().ip().is_loopback(), client.set_nodelay(true).is_ok());
    client.write_all(b"ping over tcp").unwrap();
    let mut response = Vec::new();
    client.set_read_timeout(Some(Duration::from_secs(5))).unwrap();
    client.read_to_end(&mut response).unwrap();
    println!("echo = {}", String::from_utf8(response).unwrap());
    let n = server.join().unwrap();
    println!("server saw {} bytes", n);

    // fresh listener per scenario (a listener's stream was consumed above)
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let addr3 = listener.local_addr().unwrap();
    let server2 = thread::spawn(move || {
        let (mut stream, _) = listener.accept().unwrap();
        let mut buf = Vec::new();
        stream.read_to_end(&mut buf).unwrap();
        buf.len()
    });
    let mut client2 = TcpStream::connect(addr3).unwrap();
    client2.write_all(b"bye").unwrap();
    client2.shutdown(std::net::Shutdown::Both).unwrap();
    let n2 = server2.join().unwrap();
    println!("after shutdown server saw = {}", n2);

    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let addr4 = listener.local_addr().unwrap();
    let server3 = thread::spawn(move || {
        let (mut stream, _) = listener.accept().unwrap();
        let mut buf = [0u8; 8];
        let pn = stream.peek(&mut buf).unwrap();
        let rn = stream.read(&mut buf).unwrap();
        (pn, rn)
    });
    let mut client3 = TcpStream::connect(addr4).unwrap();
    client3.write_all(b"peekme").unwrap();
    let (pn, rn) = server3.join().unwrap();
    println!("peek = {} read = {}", pn, rn);
    drop(client3);

    let dead = TcpStream::connect("127.0.0.1:1");
    println!("dead connect = {}", dead.is_err());
    drop(addr2);
}
