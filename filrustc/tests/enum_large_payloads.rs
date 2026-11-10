// Large multi-variant enums with pointer payloads: moves, matches, take,
// replace, compare, discriminants, and Result propagation of large payloads.
use std::mem::{replace, take};

#[derive(Debug, PartialEq)]
enum Shape {
    Empty,
    Text(String),
    List(Vec<Box<String>>),
    Pair { left: String, right: Box<String> },
    Words([String; 3]),
}

fn main() {
    let mut s = Shape::List(vec![Box::new("l1".into()), Box::new("l2".into())]);
    let taken = replace(&mut s, Shape::Empty);
    println!("taken empty = {:?} taken = {:?}", s, matches!(taken, Shape::List(_)));

    s = Shape::Pair { left: "L".into(), right: Box::new("R".into()) };
    let prev = replace(&mut s, Shape::Empty);
    if let Shape::Pair { left, right } = prev {
        println!("prev pair = {} {}", left, right);
    }
    println!("now = {:?}", std::mem::discriminant(&s) == std::mem::discriminant(&Shape::Empty));

    // compare large variants
    let a = Shape::Words(["a".into(), "b".into(), "c".into()]);
    let b = Shape::Words(["a".into(), "b".into(), "c".into()]);
    let c = Shape::Words(["a".into(), "b".into(), "d".into()]);
    println!("eq = {} neq = {}", a == b, a != c);

    // move enum payloads around through #[inline(never)] calls (by-value ABI)
    #[inline(never)]
    fn consume(sh: Shape) -> String {
        match sh {
            Shape::Text(t) => format!("text {}", t),
            Shape::List(l) => format!("list {}", l.iter().map(|b| b.as_str()).collect::<Vec<_>>().join("+")),
            Shape::Pair { left, right } => format!("pair {}{}", left, right),
            Shape::Words(w) => format!("words {}", w.join("|")),
            Shape::Empty => "empty".to_string(),
        }
    }
    println!("{}", consume(Shape::Text("plain".into())));
    println!("{}", consume(Shape::List(vec![Box::new("x".into()), Box::new("y".into()), Box::new("z".into())])));
    println!("{}", consume(Shape::Pair { left: "p".into(), right: Box::new("q".into()) }));
    println!("{}", consume(Shape::Words(["w1".into(), "w2".into(), "w3".into()])));
    println!("{}", consume(Shape::Empty));

    // Result propagation of large payloads with ?
    #[inline(never)]
    fn level2(bad: bool) -> Result<Shape, String> {
        if bad { Err("bad input".into()) } else { Ok(Shape::List(vec![Box::new("deep".into())])) }
    }
    #[inline(never)]
    fn level1(bad: bool) -> Result<Shape, String> {
        let inner = level2(bad)?;
        Ok(inner)
    }
    println!("ok = {}", level1(false).map(|s| consume(s)).unwrap());
    println!("err = {}", level1(true).err().unwrap());
}
