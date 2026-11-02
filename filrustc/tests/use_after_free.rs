fn main() {
    let b = Box::new(666i32);
    let p: *const i32 = &*b;
    let p = std::hint::black_box(p);
    drop(b);
    unsafe {
        println!("{}", *p);
    }
}
