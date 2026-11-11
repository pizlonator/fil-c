use std::convert::{TryFrom, TryInto};
use std::error::Error;
use std::fmt;
use std::num::{IntErrorKind, ParseIntError};

#[derive(Debug)]
struct Layer1 { msg: String }
impl fmt::Display for Layer1 {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result { write!(f, "layer1: {}", self.msg) }
}
impl Error for Layer1 {}

#[derive(Debug)]
struct Layer2 { inner: Layer1 }
impl fmt::Display for Layer2 {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result { write!(f, "layer2 wrapping") }
}
impl Error for Layer2 {
    fn source(&self) -> Option<&(dyn Error + 'static)> { Some(&self.inner) }
}

fn main() {
    // From conversions via ?
    fn build() -> Result<String, Layer2> {
        let l1 = Layer1 { msg: "bottom".to_string() };
        Err(Layer2 { inner: l1 })
    }
    let err = build().unwrap_err();
    println!("err = {}", err);
    let mut chain = 0;
    let mut cur: Option<&(dyn Error + 'static)> = Some(&err);
    while let Some(e) = cur {
        chain += 1;
        cur = e.source();
    }
    println!("chain = {}", chain);

    // Box<dyn Error> From conversions
    let boxed: Box<dyn Error> = "a str".into();
    println!("boxed str = {}", boxed);
    let boxed2: Box<dyn Error> = Layer1 { msg: "boxed layer".to_string() }.into();
    println!("boxed layer = {}", boxed2);

    // TryFrom/TryInto
    let small: u8 = u8::try_from(255i32).unwrap();
    println!("tryfrom ok = {}", small);
    let too_big = u8::try_from(300i32);
    println!("tryfrom err = {}", too_big.is_err());
    let conv: Result<u8, _> = 42i32.try_into();
    println!("tryinto = {}", conv.unwrap());
    let arr: [u8; 4] = <[u8; 4]>::try_from([1, 2, 3, 4].as_slice()).unwrap();
    println!("slice tryfrom = {:?}", arr);

    // FromStr errors with kinds
    let e = "abc".parse::<i32>().unwrap_err();
    println!("parse kind = {:?}", e.kind());
    let e2 = "999999999999999999999".parse::<i32>().unwrap_err();
    println!("overflow kind = {:?}", e2.kind() == &IntErrorKind::PosOverflow);
    let e3 = "".parse::<i32>().unwrap_err();
    println!("empty kind = {:?}", e3.kind() == &IntErrorKind::Empty);
    println!("display = {}", e);

    // error downcast through Box
    let b: Box<dyn Error> = "xyz".parse::<i32>().unwrap_err().into();
    println!("boxed parse err = {}", b);
    let as_parse = b.downcast_ref::<ParseIntError>();
    println!("downcast ok = {}", as_parse.is_some());

    // From<&str> for Box<dyn Error>
    let converted: Box<dyn Error> = "owned string error".into();
    println!("owned = {}", converted);

    // Result combinators on errors
    let r: Result<i32, ParseIntError> = "5".parse();
    let mapped = r.map_err(|e| format!("wrap: {}", e));
    println!("map_err = {:?}", mapped);
}
