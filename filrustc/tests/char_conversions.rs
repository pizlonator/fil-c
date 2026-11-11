// char conversions: char::from_u32 (valid/invalid), the safe-wrapper pattern
// for char::from_u32_unchecked, from_digit/to_digit round trips, and
// escape_debug/escape_default.
fn checked_from_u32(c: u32) -> Option<char> {
    // The safe wrapper: only hand a proven-valid scalar to the unchecked
    // constructor (from_u32_unchecked on an invalid scalar is UB).
    if char::from_u32(c).is_some() {
        Some(unsafe { char::from_u32_unchecked(c) })
    } else {
        None
    }
}

fn main() {
    println!("from_u32 = {:?}", char::from_u32(0x1F600));
    println!("from_u32 surrogate = {:?}", char::from_u32(0xD800));
    println!("from_u32 beyond = {:?}", char::from_u32(0x110000));
    println!("from_u32 nul = {:?}", char::from_u32(0));
    println!("from_u32 max = {:?}", char::from_u32(0x10FFFF));

    println!("wrapped = {} {}", checked_from_u32(65).unwrap(), checked_from_u32(0x10FFF0).unwrap());
    println!("wrapped invalid = {:?}", checked_from_u32(0xDFFF));
    println!(
        "wrapped round trip = {}",
        (0..=0x10FFFF).step_by(97).all(|c| checked_from_u32(c) == char::from_u32(c))
    );

    println!("from_digit = {:?}", char::from_digit(11, 16));
    println!("from_digit 7 = {:?}", char::from_digit(7, 8));
    println!("from_digit over radix = {:?}", char::from_digit(12, 10));
    println!("from_digit max radix = {:?}", char::from_digit(35, 36));
    println!("from_digit 0 = {:?}", char::from_digit(0, 2));

    println!("to_digit = {}", 'a'.to_digit(16).unwrap());
    println!("to_digit 7 = {}", '7'.to_digit(8).unwrap());
    println!("to_digit bad = {:?}", 'z'.to_digit(8));
    println!("to_digit max = {}", 'z'.to_digit(36).unwrap());
    println!(
        "digit round trip = {}",
        (0..16).all(|d| char::from_digit(d, 16).unwrap().to_digit(16) == Some(d))
    );
    println!(
        "digit round trip 36 = {}",
        (10..36).all(|d| char::from_digit(d, 36).unwrap().to_digit(36) == Some(d))
    );

    println!("escape_debug = {}", '\n'.escape_debug());
    println!("escape_debug quote = {}", '\''.escape_debug());
    println!("escape_debug tab = {}", '\t'.escape_debug());
    println!("escape_default = {}", '\u{7}'.escape_default());
    println!("escape_default backslash = {}", '\\'.escape_default());
    println!("escape_default plain = {}", 'k'.escape_default());
    println!("is_control = {}", '\u{0}'.is_control());
    println!("upper = {}", 'ß'.to_uppercase().to_string());
    println!("lower = {}", 'Å'.to_lowercase().next().unwrap());
}
