// mpsc channels carrying large pointer-containing messages between threads;
// deterministic because messages carry a sequence number and are sorted.
use std::sync::mpsc;
use std::thread;

struct Message {
    seq: u64,
    sender: u32,
    payload: Vec<Box<String>>,
}

impl Message {
    fn new(seq: u64, sender: u32) -> Message {
        Message {
            seq,
            sender,
            payload: (0..3).map(|i| Box::new(format!("s{}-m{}-p{}", sender, seq, i))).collect(),
        }
    }

    fn render(&self) -> String {
        format!("{} from {}", self.payload.iter().map(|b| b.as_str()).collect::<Vec<_>>().join("+"), self.sender)
    }
}

fn main() {
    let (tx, rx) = mpsc::channel::<Message>();
    let mut handles = Vec::new();
    for sender in 0..3u32 {
        let tx = tx.clone();
        handles.push(thread::spawn(move || {
            for seq in 0..4u64 {
                tx.send(Message::new(seq, sender)).unwrap();
            }
        }));
    }
    drop(tx);

    let mut got: Vec<String> = Vec::new();
    for m in rx {
        got.push(format!("{}: {}", m.seq, m.render()));
    }
    got.sort();
    println!("count = {}", got.len());
    for line in got.iter().take(3) {
        println!("first3: {}", line);
    }
    println!("last = {}", got[got.len() - 1]);

    // sync_channel with bounded capacity and large payloads
    let (stx, srx) = mpsc::sync_channel::<Message>(2);
    let stx2 = stx.clone();
    let h1 = thread::spawn(move || {
        for seq in 0..3u64 {
            stx.send(Message::new(seq, 100)).unwrap();
        }
    });
    let h2 = thread::spawn(move || {
        for seq in 10..12u64 {
            stx2.send(Message::new(seq, 200)).unwrap();
        }
    });
    let mut sync_got: Vec<u64> = Vec::new();
    for m in srx {
        sync_got.push(m.seq);
    }
    sync_got.sort();
    println!("sync = {}", sync_got.iter().map(|s| s.to_string()).collect::<Vec<_>>().join(","));
    h1.join().unwrap();
    h2.join().unwrap();
}
