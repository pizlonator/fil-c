// #[repr(C)] unions with pointer fields moved through MEMORY: swap, replace,
// reference reads, structs containing unions passed by value, and bare
// unions passed by value across Rust-ABI call boundaries (Vec::push and the
// `for u in v` drain).  Regression guard for change #23 -- the Rust-ABI
// by-value union fix: pointer-bearing unions now pass indirectly, so the
// capability travels with the payload.  The `extern "C"` by-value union
// variant is the remaining documented gap (known-issues #10) and is
// deliberately NOT exercised here.
#[repr(C)]
union U {
    p: *mut u8,
    i: u64,
}

#[repr(C)]
struct Wrap {
    u: U,
    extra: u64,
}

#[inline(never)]
fn read_ref(u: &U) -> u64 {
    // SAFETY: the union was written through `p` and its pointee is a live
    // boxed u64 for the whole test.
    unsafe { *(u.p as *const u64) }
}

#[inline(never)]
fn consume(w: Wrap) -> u64 {
    // SAFETY: as above; `w` owns its union's pointee until the caller drops it.
    unsafe { *(w.u.p as *const u64) }
}

#[inline(never)]
fn make_wrap(tag: u64) -> Wrap {
    Wrap { u: U { p: Box::into_raw(Box::new(tag)) as *mut u8 }, extra: tag * 100 }
}

fn main() {
    // unions built inline in main (no by-value union ABI moves)
    let tag1 = Box::into_raw(Box::new(11u64)) as *mut u8;
    let tag2 = Box::into_raw(Box::new(22u64)) as *mut u8;
    let mut a = U { p: tag1 };
    let mut b = U { p: tag2 };
    println!("before: {} {}", unsafe { *(a.p as *const u64) }, unsafe { *(b.p as *const u64) });

    std::mem::swap(&mut a, &mut b);
    println!("after: {} {}", unsafe { *(a.p as *const u64) }, unsafe { *(b.p as *const u64) });
    println!("via ref: {} {}", read_ref(&a), read_ref(&b));
    unsafe { drop(Box::from_raw(a.p)); drop(Box::from_raw(b.p)); }

    // mem::replace on a union local
    let mut c = U { p: Box::into_raw(Box::new(33u64)) as *mut u8 };
    let old = std::mem::replace(&mut c, U { p: Box::into_raw(Box::new(44u64)) as *mut u8 });
    println!("replace: {} {}", unsafe { *(c.p as *const u64) }, unsafe { *(old.p as *const u64) });
    unsafe { drop(Box::from_raw(c.p)); drop(Box::from_raw(old.p)); }

    // a struct containing a union moves by value safely (typed allocas)
    let w = make_wrap(66);
    let inner = unsafe { w.u.p };
    let extra = w.extra;
    println!("consume = {}", consume(w));
    println!("extra = {}", extra);
    unsafe { drop(Box::from_raw(inner)); }

    // unions inside a Vec, shuffled with swap
    let mut v: Vec<U> = Vec::new();
    for tag in 0..4u64 {
        v.push(U { p: Box::into_raw(Box::new(tag * 10)) as *mut u8 });
    }
    {
        let (l, r) = v.split_at_mut(2);
        std::mem::swap(&mut l[0], &mut r[1]);
    }
    let vals: Vec<u64> = v.iter().map(|u| unsafe { *(u.p as *const u64) }).collect();
    println!("vec vals: {} {:?}", vals.iter().map(|x| x.to_string()).collect::<Vec<_>>().join(","), vals == vec![30, 10, 20, 0]);
    for u in v {
        unsafe { drop(Box::from_raw(u.p)); }
    }
}
