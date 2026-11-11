use std::str::FromStr;

fn main() {
    println!("i32 = {:?}", i32::from_str("-42").unwrap());
    println!("u64 = {:?}", u64::from_str("18446744073709551615").unwrap());
    println!("f64 = {:?}", f64::from_str("3.25").unwrap());
    println!("bool = {:?}", bool::from_str("true").unwrap());
    println!("bad = {} {} {}", "x".parse::<i32>().is_err(), "1.5".parse::<i32>().is_err(), "9223372036854775808".parse::<i64>().is_err());
    println!("plus = {:?}", "+7".parse::<i32>().unwrap());
    println!("radix = {:?} {:?} {:?}", i64::from_str_radix("ff", 16).unwrap(), i64::from_str_radix("101", 2).unwrap(), u32::from_str_radix("-1", 10).is_err());
    println!("exp = {:?}", "1e3".parse::<f64>().unwrap());
    println!("negzero = {}", ("-0.0".parse::<f64>().unwrap()).is_sign_negative());
    println!("inf = {:?}", "inf".parse::<f64>().unwrap());
    println!("nan = {}", "NaN".parse::<f64>().unwrap().is_nan());
    println!("i128 = {:?}", i128::from_str("170141183460469231731687303715884105727").unwrap());
    println!("u128 = {:?}", u128::from_str_radix("ffffffffffffffffffffffffffffffff", 16).unwrap());
    println!("usize = {}", usize::from_str("99").unwrap());
    println!("isize = {}", isize::from_str("-5").unwrap());
    println!("f32 = {}", f32::from_str("0.5").unwrap());
    println!("char parse via u8 = {}", u8::from_str("255").unwrap());
    println!("leading zero = {}", u32::from_str("007").unwrap());
    println!("empty = {}", "".parse::<i32>().is_err());
}
