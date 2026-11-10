// String operations that move bytes and grow allocations; the String struct
// itself (ptr/cap/len) moves through typed allocas everywhere here.
fn main() {
    let mut s = String::with_capacity(4);
    s.push_str("hello");
    s.push(' ');
    s.push_str("filc");
    println!("s = {} (cap >= 8: {})", s, s.capacity() >= 8);

    s.insert_str(0, ">>");
    println!("inserted = {}", s);

    let mut replaced = s.replace("filc", "rust");
    println!("replaced = {}", replaced);

    let drained: String = replaced.drain(2..4).collect();
    println!("drained = {:?} left = {:?}", drained, replaced);

    // round-trip through bytes
    let bytes = s.clone().into_bytes();
    let back = String::from_utf8(bytes).unwrap();
    println!("utf8 = {} same = {}", back, back == s);

    // build a long string in chunks (forces Vec growth + memmoves)
    let mut big = String::new();
    for i in 0..50 {
        big.push_str(&format!("chunk{};", i));
    }
    println!("big len = {} starts {}", big.len(), &big[..12]);

    // split + collect + join
    let parts: Vec<&str> = big.split(';').filter(|p| !p.is_empty()).collect();
    println!("parts = {} first = {}", parts.len(), parts[0]);
    let rejoined = parts.join("|");
    println!("rejoined starts {} len {}", &rejoined[..16], rejoined.len());

    // String -> Box<str> -> String
    let boxed: Box<str> = s.clone().into_boxed_str();
    let unboxed: String = boxed.into_string();
    println!("boxed roundtrip same = {}", unboxed == s);

    // chars iteration and char-level ops on a string with multi-byte chars
    let uni = String::from("héllo");
    let upper = uni.to_uppercase();
    println!("uni = {} upper = {} chars = {}", uni, upper, uni.chars().count());
}
