use std::fmt;
use std::net::SocketAddr;

#[derive(Debug)]
struct Point { x: i32, y: i32 }

impl fmt::Display for Point {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "({}, {})", self.x, self.y)
    }
}

fn main() {
    let p = Point { x: 3, y: -4 };
    println!("display = {}", p);
    println!("debug = {:?}", p);
    println!("debug pretty = {:#?}", p);

    // width/fill/alignment
    println!("[{:5}] [{:05}] [{:<5}] [{:>5}] [{:^5}] [{:*^7}]", 42, 42, 42, 42, 42, 42);
    // sign and alternate
    println!("[{:+}] [{:+}] [{:#x}] [{:#o}] [{:#b}]", 5, -5, 255, 8, 6);
    // precision
    println!("[{:.2}] [{:.0}]", 3.14159, 3.14159);
    // strings width and precision
    println!("[{:8}] [{:<8}] [{:.3}] [{:8.3}]", "hi", "hi", "hello", "hello");
    println!("escapes = {{}} {{}}", );
    println!("positional = {0} {1} {0}", "a", "b");
    println!("named = {name} {n}", name = "named", n = 9);

    // Debug for standard types
    println!("vec debug = {:?}", vec![1, 2, 3]);
    println!("option debug = {:?} {:?}", Some(5), None::<u8>);
    println!("result debug = {:?}", Ok::<i32, &str>(7));
    println!("string debug = {:?}", "quoted \"str\"");
    println!("char debug = {:?}", 'x');
    println!("tuple debug = {:?}", (1, "two", 3.0));
    println!("range debug = {:?}", 0..5);
    println!("slice debug = {:?}", &[1u8, 2][..]);

    // Display for Path and SocketAddr
    println!("path display = {}", std::path::Path::new("/tmp/x/y.txt").display());
    let sa: SocketAddr = "127.0.0.1:9000".parse().unwrap();
    println!("socketaddr display = {} debug = {:?}", sa, sa);

    // write! into a String
    let mut buf = String::new();
    fmt::write(&mut buf, format_args!("write! {} {}", 10, "ok")).unwrap();
    println!("fmt::write = {}", buf);
    let formatted = format!("{:=>10}", "x");
    println!("format = {}", formatted);
}
