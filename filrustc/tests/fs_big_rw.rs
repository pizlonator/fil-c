// The >8 KiB multi-chunk regime of the fixed default_read_to_end (change #25):
// a 64 KiB file read back byte-identically through fs::read, read_to_string
// and File::read_to_end, plus the File::read_buf contract through std's own
// stable io::copy loops (the Fil-C read_buf hands the kernel a plain
// 8192-byte scratch buffer and cursor.append()s the bytes, so every single
// read_buf call transfers at most 8192 bytes and std must loop; the counting
// sink below pins that cap, which direct read_buf calls cannot -- the trait
// method is unstable on the release channel, E0554).
use std::fs::File;
use std::io::{copy, prelude::*, BufWriter, Seek, SeekFrom, Write};

struct CountingSink {
    writes: usize,
    max_chunk: usize,
    total: usize,
}

impl Write for CountingSink {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        self.writes += 1;
        if buf.len() > self.max_chunk {
            self.max_chunk = buf.len();
        }
        self.total += buf.len();
        Ok(buf.len())
    }
    fn flush(&mut self) -> std::io::Result<()> {
        Ok(())
    }
}

// Deterministic ASCII pattern: 1024 rows of exactly 64 bytes (65536 total).
fn build_pattern(rows: usize) -> String {
    let alphabet = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    let prefix_len = 6; // "NNNNN:"
    let mut s = String::with_capacity(rows * 64);
    for row in 0..rows {
        s.push_str(&format!("{:05}:", row));
        for k in 0..(64 - prefix_len) {
            let idx = (row * 7 + k * 13) % alphabet.len();
            s.push(alphabet[idx] as char);
        }
    }
    s
}

fn main() {
    let dir = format!("/tmp/filc-big-{}", std::process::id());
    std::fs::create_dir_all(&dir).unwrap();
    let path = format!("{}/big.dat", dir);
    let expected = build_pattern(1024);
    println!("built len = {}", expected.len());

    {
        let mut f = File::create(&path).unwrap();
        f.write_all(expected.as_bytes()).unwrap();
        f.sync_all().unwrap();
    }
    println!("on disk = {}", std::fs::metadata(&path).unwrap().len() == expected.len() as u64);

    // fs::read: the fixed Vec::with_capacity + default_read_to_end path,
    // multi-chunk (the 32-byte probe grows through many 8 KiB reads).
    let bytes = std::fs::read(&path).unwrap();
    println!("fs::read len = {}", bytes.len());
    println!("fs::read identical = {}", bytes == expected.as_bytes());

    // read_to_string (same fixed default_read_to_end machinery).
    let s = std::fs::read_to_string(&path).unwrap();
    println!("read_to_string identical = {}", s == expected);

    // File::read_to_end into an empty Vec.
    let mut v = Vec::new();
    File::open(&path).unwrap().read_to_end(&mut v).unwrap();
    println!("read_to_end identical = {}", v == expected.as_bytes());

    // io::copy into a counting sink: generic_copy's stack-buffer loop drives
    // File::read_buf repeatedly until it appends nothing, handing each
    // read_buf's bytes to write_all as one chunk -- so chunks == 8 with every
    // chunk exactly 8192 pins the 8192-byte-per-call cap and the must-loop
    // contract for a 64 KiB file.
    let mut f = File::open(&path).unwrap();
    let mut sink = CountingSink { writes: 0, max_chunk: 0, total: 0 };
    let n = copy(&mut f, &mut sink).unwrap();
    println!(
        "copy bytes = {} chunks = {} max chunk = {} total = {}",
        n, sink.writes, sink.max_chunk, sink.total
    );
    println!(
        "cap respected = {}",
        sink.writes == 8 && sink.max_chunk == 8192 && sink.total == expected.len() as usize
    );

    // io::copy into a 16384-capacity BufWriter: the read_buf cursor capacity
    // exceeds the cap, so std's BufWriter loop must call read_buf twice per
    // window (the caller loops around the cap).
    let mut f = File::open(&path).unwrap();
    let mut sink_buf = BufWriter::with_capacity(16384, Vec::new());
    let n2 = copy(&mut f, &mut sink_buf).unwrap();
    let buf: Vec<u8> = sink_buf.into_inner().unwrap();
    println!("bufwriter copy = {} identical = {}", n2, buf == expected.as_bytes());

    // Positional reads around the tail through a fresh handle.
    let mut f = File::open(&path).unwrap();
    f.seek(SeekFrom::End(-7)).unwrap();
    let mut tail = [0u8; 7];
    f.read_exact(&mut tail).unwrap();
    println!(
        "tail matches = {}",
        String::from_utf8_lossy(&tail) == &expected[expected.len() - 7..]
    );

    std::fs::remove_file(&path).unwrap();
    std::fs::remove_dir_all(&dir).unwrap();
    println!("cleaned = {}", !std::path::Path::new(&dir).exists());
}
