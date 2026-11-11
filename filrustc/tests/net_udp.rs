use std::net::UdpSocket;
use std::thread;
use std::time::Duration;

fn main() {
    let a = UdpSocket::bind("127.0.0.1:0").unwrap();
    let b = UdpSocket::bind("127.0.0.1:0").unwrap();
    let a_addr = a.local_addr().unwrap();
    let b_addr = b.local_addr().unwrap();
    println!("addrs loopback = {} {}", a_addr.ip().is_loopback(), b_addr.ip().is_loopback());

    let rx = thread::spawn(move || {
        let mut buf = [0u8; 128];
        let (n, from) = b.recv_from(&mut buf).unwrap();
        (n, String::from_utf8(buf[..n].to_vec()).unwrap(), from)
    });
    a.send_to(b"udp hello", b_addr).unwrap();
    let (n, msg, from) = rx.join().unwrap();
    println!("recv = {} {} same-origin = {}", n, msg, from == a_addr);

    // connected socket: send + recv
    let mut a2 = UdpSocket::bind("127.0.0.1:0").unwrap();
    let b2 = UdpSocket::bind("127.0.0.1:0").unwrap();
    let b2_addr = b2.local_addr().unwrap();
    a2.connect(b2_addr).unwrap();
    let b3 = b2.try_clone().unwrap();
    let rx2 = thread::spawn(move || {
        let mut buf = [0u8; 128];
        let (n, from) = b2.recv_from(&mut buf).unwrap();
        let payload = String::from_utf8(buf[..n].to_vec()).unwrap();
        (n, payload, from)
    });
    a2.send(b"connected udp").unwrap();
    let (n2, payload2, _) = rx2.join().unwrap();
    println!("connected send = {} {}", n2, payload2);

    // b2 replies to a2's address; a2 (connected) receives it
    let a2_addr = a2.local_addr().unwrap();
    let (tx, rx) = std::sync::mpsc::channel();
    thread::spawn(move || {
        let mut buf = [0u8; 128];
        a2.set_read_timeout(Some(Duration::from_secs(3))).unwrap();
        let r = match a2.recv_from(&mut buf) {
            Ok((n, _)) => Some(String::from_utf8(buf[..n].to_vec()).unwrap()),
            Err(_) => None,
        };
        tx.send(r).unwrap();
    });
    b3.send_to(b"reply!", a2_addr).unwrap();
    let got = rx.recv().unwrap();
    println!("reply = {}", got.unwrap());

    println!("local again = {}", UdpSocket::bind("127.0.0.1:0").unwrap().local_addr().unwrap().port() > 0);
}
