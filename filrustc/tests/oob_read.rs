fn main() {
    let v: Vec<i32> = vec![1, 2, 3];
    let p: *const i32 = v.as_ptr();
    let p = std::hint::black_box(p);
    unsafe {
        println!("{}", *p.add(1000000000));
    }
}
