fn main() {
    let mut v: Vec<i32> = vec![1, 2, 3];
    let p: *mut i32 = v.as_mut_ptr();
    let p = std::hint::black_box(p);
    unsafe {
        *p.add(1000000000) = 666;
    }
    println!("Should not get here.");
}
