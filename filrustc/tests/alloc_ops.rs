use std::alloc::{alloc, dealloc, realloc, Layout};
use std::ptr;

fn main() {
    // Box::into_raw/from_raw round trip
    let b = Box::new(vec!["alloc".to_string(), "ops".to_string()]);
    let raw = Box::into_raw(b);
    let recovered = unsafe { Box::from_raw(raw) };
    println!("box roundtrip = {} {}", recovered[0], recovered[1]);

    let big = Box::new([0u64; 8]);
    let raw2 = Box::into_raw(big);
    unsafe {
        let slice = &*raw2;
        println!("big len = {} sum = {}", slice.len(), slice.iter().sum::<u64>());
        drop(Box::from_raw(raw2));
    }

    // std::alloc raw alloc/dealloc/realloc
    unsafe {
        let layout = Layout::from_size_align(64, 8).unwrap();
        let p1 = alloc(layout);
        assert!(!p1.is_null());
        ptr::write_bytes(p1, 0xAB, 64);
        let byte = ptr::read(p1);
        println!("raw alloc byte = {:#x}", byte);
        let p2 = realloc(p1, layout, 128);
        assert!(!p2.is_null());
        ptr::write(p2.add(64), 0xCD_u8);
        println!("realloc byte at 64 = {:#x}", ptr::read(p2.add(64)));
        dealloc(p2, Layout::from_size_align(128, 8).unwrap());
        println!("raw dealloc ok");
    }

    // Vec reserve/shrink/with_capacity/leak
    let mut v: Vec<Box<String>> = Vec::with_capacity(2);
    println!("cap = {}", v.capacity());
    for i in 0..20 {
        v.push(Box::new(format!("v{}", i)));
    }
    println!("len = {} cap >= len = {}", v.len(), v.capacity() >= v.len());
    v.shrink_to_fit();
    println!("shrunk cap = {}", v.capacity());
    v.reserve(100);
    println!("reserved cap >= 120 = {}", v.capacity() >= 120);
    let leaked: &'static mut [Box<String>] = Vec::leak(v);
    println!("leaked = {} {}", leaked.len(), leaked[19]);
    // leak is intentional; do not free (bounded number of leaked elements)
}
