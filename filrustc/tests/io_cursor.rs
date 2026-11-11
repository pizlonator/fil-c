use std::io::{BufRead, Cursor, Read, Seek, SeekFrom, Write};

fn main() {
    let mut c = Cursor::new(Vec::new());
    c.write_all(b"hello filc cursor").unwrap();
    writeln!(c, " line2").unwrap();
    write!(c, "num = {}", 42).unwrap();
    println!("written = {} bytes", c.position());
    c.seek(SeekFrom::Start(0)).unwrap();

    let mut s = String::new();
    c.read_to_string(&mut s).unwrap();
    println!("read = {}", s.trim_end());

    let data = c.into_inner();
    let mut c = Cursor::new(data.clone());
    c.seek(SeekFrom::Start(0)).unwrap();
    let mut line = String::new();
    c.read_line(&mut line).unwrap();
    println!("line1 = {}", line.trim_end());

    c.seek(SeekFrom::Start(6)).unwrap();
    let mut buf = [0u8; 4];
    c.read_exact(&mut buf).unwrap();
    println!("exact = {}", String::from_utf8(buf.to_vec()).unwrap());

    c.seek(SeekFrom::End(-2)).unwrap();
    let mut rest = Vec::new();
    c.read_to_end(&mut rest).unwrap();
    println!("tail = {}", String::from_utf8(rest).unwrap());

    let mut c = Cursor::new(data.clone());
    c.seek(SeekFrom::Start(0)).unwrap();
    let split: Vec<String> = c.split(b' ').map(|r| String::from_utf8(r.unwrap()).unwrap()).collect();
    println!("split = {}", split.join("|"));

    let mut c = Cursor::new(data.clone());
    c.seek(SeekFrom::Start(0)).unwrap();
    let bytes: Vec<u8> = c.bytes().take(5).map(|r| r.unwrap()).collect();
    println!("bytes = {:?}", String::from_utf8(bytes).unwrap());

    let mut c = Cursor::new(data.clone());
    c.seek(SeekFrom::Start(0)).unwrap();
    let until: Vec<u8> = {
        let mut out = Vec::new();
        c.read_until(b' ', &mut out).unwrap();
        out
    };
    println!("until space = {}", String::from_utf8(until).unwrap());

    let mut ro = Cursor::new(vec![1u8, 2, 3, 4]);
    let mut out = Vec::new();
    std::io::copy(&mut ro, &mut out).unwrap();
    println!("copied = {:?}", out);
}
