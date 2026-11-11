use std::io::{prelude::*, BufReader, BufWriter, Cursor};

fn main() {
    let data = "alpha\nbeta\ngamma\ndelta\n".to_string().into_bytes();
    let mut reader = BufReader::new(Cursor::new(data.clone()));
    let mut first = String::new();
    reader.read_line(&mut first).unwrap();
    let mut lines = vec![first.trim_end().to_string()];
    for line in reader.lines() {
        lines.push(line.unwrap());
    }
    println!("lines = {}", lines.join(","));

    let mut reader = BufReader::new(Cursor::new(data.clone()));
    let mut chunk = [0u8; 6];
    reader.read_exact(&mut chunk).unwrap();
    println!("chunk = {}", String::from_utf8(chunk.to_vec()).unwrap());
    println!("buffered = {}", reader.buffer().len());
    let filled = reader.fill_buf().unwrap().to_vec();
    println!("fill_buf = {}", String::from_utf8(filled).unwrap());

    let mut writer = BufWriter::new(Cursor::new(Vec::new()));
    write!(writer, "formatted {}", 7).unwrap();
    writeln!(writer, " done").unwrap();
    writer.flush().unwrap();
    let inner = writer.into_inner().unwrap();
    let finished = inner.into_inner();
    println!("bufwriter = {}", String::from_utf8(finished).unwrap().trim_end().to_string());

    let mut cursor = Cursor::new(data.clone());
    {
        let mut by_ref = Read::by_ref(&mut cursor).take(5);
        let mut taken = Vec::new();
        by_ref.read_to_end(&mut taken).unwrap();
        println!("take(5) = {}", String::from_utf8(taken).unwrap());
    }
    let mut all = String::new();
    cursor.read_to_string(&mut all).unwrap();
    println!("read_to_string = {}", all.trim_end().replace('\n', "|"));

    let a = Cursor::new(b"ab".to_vec());
    let b = Cursor::new(b"cd".to_vec());
    let mut chained = a.chain(b);
    let mut whole = Vec::new();
    chained.read_to_end(&mut whole).unwrap();
    println!("chained = {}", String::from_utf8(whole).unwrap());

    let mut slice_reader = BufReader::new(&data[..]);
    let mut first_word = Vec::new();
    slice_reader.read_until(b'\n', &mut first_word).unwrap();
    println!("first word = {}", String::from_utf8(first_word).unwrap().trim_end().to_string());
}
