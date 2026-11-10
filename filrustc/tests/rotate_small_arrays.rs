// slice::rotate_left / rotate_right across the risky regimes:
// - 8-byte pointer elements (Box) on small FIXED arrays where the rotate's
//   branch conditions and copy lengths are compile-time constants after
//   inlining (the shape that would let SROA widen the rotate scratch's
//   memcpys if the scratch were ever typed as an integer partition),
// - 40-byte elements ([u8; 40]) which span the [usize; 32] scratch threshold,
// - runtime-count rotates on Vec<String>.
fn show_boxes(v: &[Box<String>]) -> String {
    v.iter().map(|b| b.as_str()).collect::<Vec<_>>().join(",")
}

fn show_bytes(arr: &[[u8; 40]]) -> String {
    arr.iter().map(|a| (a[0] as char).to_string()).collect::<Vec<_>>().join(",")
}

fn main() {
    // constant fold regime: rotate_left(1) / rotate_right(1) on 8-byte elements
    let mut a1: [Box<String>; 5] = [
        Box::new("a".to_string()),
        Box::new("b".to_string()),
        Box::new("c".to_string()),
        Box::new("d".to_string()),
        Box::new("e".to_string()),
    ];
    a1[..].rotate_left(1);
    println!("rl1 = {}", show_boxes(&a1));
    a1[..].rotate_right(1);
    println!("rr1 = {}", show_boxes(&a1));

    // constant fold regime: rotate by 2 and by len-1
    let mut a2: [Box<String>; 4] = [
        Box::new("1".to_string()),
        Box::new("2".to_string()),
        Box::new("3".to_string()),
        Box::new("4".to_string()),
    ];
    a2[..].rotate_left(2);
    println!("rl2 = {}", show_boxes(&a2));
    a2[..].rotate_left(3);
    println!("rl3 = {}", show_boxes(&a2));

    // 40-byte elements: the rotate memmove scratch path with count < len
    let mut bytes: [[u8; 40]; 6] = [[b'A'; 40], [b'B'; 40], [b'C'; 40], [b'D'; 40], [b'E'; 40], [b'F'; 40]];
    for i in 0..6 { bytes[i][0] = b'a' + i as u8; bytes[i][39] = b'0' + i as u8; }
    bytes.rotate_left(2);
    println!("bytes40 = {} tails {}", show_bytes(&bytes),
        bytes.iter().map(|a| (a[39] - b'0').to_string()).collect::<Vec<_>>().join(""));

    // runtime-count rotates on a Vec of pointer-containing structs
    let v: Vec<Box<String>> = (0..7).map(|i| Box::new(format!("v{}", i))).collect();
    for k in [0usize, 1, 3, 6, 7] {
        let mut u = v.clone();
        u[..].rotate_left(k);
        println!("rl({}) = {}", k, show_boxes(&u));
    }

    // rotate_right with runtime count
    let mut w: Vec<String> = (0..5).map(|i| format!("s{}", i)).collect();
    w.rotate_right(2);
    println!("rr(2) = {}", w.join(","));
}
