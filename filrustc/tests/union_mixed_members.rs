// Mirror of filc/tests/wackyunion (C): a union whose members mix integers,
// data pointers and function pointers, copy-moved through memory and
// dispatched through its function-pointer member.  clang compiles this shape
// by escaping every union-carrying alloca with `zhas_union` and moving the
// bytes with `zmemmove_builtin`; the Fil-C rustc does exactly the same (see
// the known-issue #13 notes in build_rustc-notes.md), so every member --
// including a function pointer -- must come back out of whole-aggregate
// copies, swaps and Vec moves with its capability intact.
#[repr(C)]
#[derive(Clone, Copy)]
union Wacky {
    as_int: usize,
    as_ptr: *mut u8,
    as_fn: fn(&mut u64) -> u64,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct Wrap {
    tag: u32,
    pad: u32,
    w: Wacky,
}

#[inline(never)]
fn probe(x: &mut u64) -> u64 {
    *x * 3
}

#[inline(never)]
fn copy_wraps(src: &[Wrap; 3]) -> [Wrap; 3] {
    *src
}

fn main() {
    let mut n = 7u64;
    let table = [
        Wrap { tag: 0, pad: 0, w: Wacky { as_fn: probe } },
        Wrap { tag: 1, pad: 0, w: Wacky { as_ptr: (&mut n as *mut u64).cast::<u8>() } },
        Wrap { tag: 2, pad: 0, w: Wacky { as_int: 4242 } },
    ];
    // whole-array aggregate copy of union-carrying data
    let mut t2 = copy_wraps(&table);
    // swap two entries: mem::swap moves both unions through scratch memory
    t2.swap(0, 2);
    // entry 0 now holds the integer, entry 2 the function pointer
    let r0 = unsafe { t2[0].w.as_int };
    let r2 = unsafe { (t2[2].w.as_fn)(&mut n) };
    // write through the restored data pointer, then read it back
    unsafe { *(t2[1].w.as_ptr as *mut u64) = 99 };
    let r1 = unsafe { *(t2[1].w.as_ptr as *const u64) };
    println!("{} {} {} {}", r0, r2, r1, n);
    assert_eq!(r0, 4242);
    assert_eq!(r2, 21);
    assert_eq!(r1, 99);
    assert_eq!(n, 99);

    // unions by value into a Vec and back out (push moves the union; the
    // element reads go through reference-based union reads)
    let mut v: Vec<Wacky> = Vec::new();
    v.push(Wacky { as_int: 12345 });
    v.push(Wacky { as_fn: probe });
    v.push(Wacky { as_ptr: (&mut n as *mut u64).cast::<u8>() });
    let a = unsafe { v[0].as_int };
    let b = unsafe { (v[1].as_fn)(&mut n) };
    unsafe { *(v[2].as_ptr as *mut u64) = 555 };
    let c = unsafe { *(v[2].as_ptr as *const u64) };
    println!("{} {} {} {}", a, b, c, n);
    assert_eq!(a, 12345);
    assert_eq!(b, 297);
    assert_eq!(c, 555);
    assert_eq!(n, 555);
}
