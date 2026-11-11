fn main() {
    println!("wrapping = {} {}", 250u8.wrapping_add(10), (i32::MIN).wrapping_sub(1) == i32::MAX);
    println!("saturating = {} {} {}", 250u8.saturating_add(10), 1i32.saturating_sub(5), (i32::MAX).saturating_add(1));
    println!("checked = {:?} {:?} {:?}", 5i32.checked_add(3), 5i32.checked_mul(1 << 30), 1u8.checked_sub(2));
    println!("pow = {} {}", 2i32.pow(10), 2i64.checked_pow(62).unwrap());
    println!("overflow pow = {:?}", 2i32.checked_pow(31));
    println!("abs = {} {} {}", (-5i32).abs(), i32::MIN.checked_abs().is_none(), (-5.5f64).abs());
    println!("signum = {} {}", (-7i32).signum(), 9i64.signum());
    println!("div_euclid = {} rem_euclid = {}", (-7i32).div_euclid(2), (-7i32).rem_euclid(2));
    println!("u div_euclid = {}", 7u32.div_euclid(2));
    println!("div ceil = {} {}", (7u64 + 1) / 2, 10u64 / 4);
    println!("bits = {} {} {} {}", 1u32.leading_zeros(), 1u32.trailing_zeros(), 0b1010u32.count_ones(), 0xFFu16.swap_bytes());
    println!("rotate = {}", 0x01020304u32.rotate_left(8));
    println!("bytes = {:?} <- {:?}", 0x11223344u32.to_le_bytes(), u32::from_le_bytes([0x44, 0x33, 0x22, 0x11]));
    println!("be = {:?}", u32::from_be_bytes(0x11223344u32.to_be_bytes()));
    println!("i128 mul = {}", (1i128 << 100) / (1i128 << 50));
    println!("u128 add = {}", (u128::MAX - 1).checked_add(1).unwrap());
    println!("rem = {} {} {}", 5 % 3, 5u32 % 3, (-5i32).rem_euclid(3));
    println!("minmax = {} {} {}", i64::MIN, i64::MAX, u64::MAX);
    println!("from bool = {}", u8::from(true));
    println!("tryfrom int = {:?}", u8::try_from(300i32).is_err());
    println!("casting = {} {}", 300i32 as u8, -1i32 as u32);
    println!("swap = {}", 0x1122334455667788u64.swap_bytes());
    println!("ilog = {} {}", 100u32.ilog2(), 1000u32.ilog10());
    println!("is_power_of_two = {} {}", 64u64.is_power_of_two(), 63u64.is_power_of_two());
    println!("next_power_of_two = {}", 63u32.next_power_of_two());
    println!("abs_diff = {}", 3i32.abs_diff(10));
}
