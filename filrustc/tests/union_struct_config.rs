// Mirror of filc/tests/uniontest (C, based on PostgreSQL's config_var_value):
// a struct holding a union of scalar and pointer members plus an extra
// pointer field, updated field-by-field through a dispatch and moved BY VALUE
// through #[inline(never)] call boundaries (returns, Vec::push, mem::replace).
// clang escapes the union-carrying allocas of this shape with `zhas_union`
// and moves the bytes with `zmemmove_builtin`; the Fil-C rustc mirrors that,
// so the string pointer's and the extra pointer's capabilities must survive
// every move.
#[repr(C)]
union CvvVal {
    boolval: bool,
    intval: i64,
    stringval: *mut u8,
}

#[repr(C)]
struct ConfigVarValue {
    val: CvvVal,
    // points to a boxed u64 holding the string length (the C test's `extra`)
    extra: *mut u8,
}

#[inline(never)]
fn boxed_len(len: u64) -> *mut u8 {
    Box::into_raw(Box::new(len)) as *mut u8
}

#[inline(never)]
fn make_value(kind: u32, payload: u64) -> ConfigVarValue {
    match kind {
        0 => ConfigVarValue { val: CvvVal { boolval: payload != 0 }, extra: boxed_len(0) },
        1 => ConfigVarValue { val: CvvVal { intval: payload as i64 }, extra: boxed_len(0) },
        _ => {
            let text = format!("v{}", payload).into_bytes();
            let len = text.len() as u64;
            let buf = Box::into_raw(text.into_boxed_slice()) as *mut u8;
            ConfigVarValue { val: CvvVal { stringval: buf }, extra: boxed_len(len) }
        }
    }
}

#[inline(never)]
fn clone_string_value(v: &ConfigVarValue) -> ConfigVarValue {
    // reads through v.val.stringval and v.extra; the capability of both
    // pointers must survive the &-passing and the by-value return.
    let len = unsafe { *(v.extra as *const u64) } as usize;
    let mut new_buf: Vec<u8> = vec![0u8; len];
    for i in 0..len {
        new_buf[i] = unsafe { *v.val.stringval.add(i) };
    }
    ConfigVarValue {
        val: CvvVal { stringval: Box::into_raw(new_buf.into_boxed_slice()) as *mut u8 },
        extra: boxed_len(len as u64),
    }
}

#[inline(never)]
fn drop_value(v: ConfigVarValue, is_string: bool) {
    unsafe {
        if is_string {
            let len = *(v.extra as *const u64) as usize;
            drop(Box::from_raw(std::slice::from_raw_parts_mut(v.val.stringval, len)));
        }
        drop(Box::from_raw(v.extra as *mut u64));
    }
}

fn main() {
    let a = make_value(0, 0); // bool false
    let b = make_value(1, 100); // int 100
    let c = make_value(2, 7); // string "v7", len 2
    // by-value moves through a Vec (each push is an aggregate copy)
    let mut v: Vec<ConfigVarValue> = Vec::new();
    v.push(a);
    v.push(b);
    v.push(c);
    // mem::replace moves the old value out by value
    let taken = std::mem::replace(&mut v[0], make_value(1, 500));
    println!("bool = {}", unsafe { taken.val.boolval });
    println!("int = {}", unsafe { v[0].val.intval });
    // field-by-field updates through the union (the uniontest dispatch)
    let cur = unsafe { v[1].val.intval };
    v[1].val.intval = cur + 666;
    println!("int2 = {}", unsafe { v[1].val.intval });
    // string member: clone through a chain of by-value moves
    let s = clone_string_value(&v[2]);
    let s2 = clone_string_value(&s);
    let len = unsafe { *(s2.extra as *const u64) };
    let text = unsafe {
        std::str::from_utf8(std::slice::from_raw_parts(s2.val.stringval, len as usize)).unwrap()
    };
    println!("str = {} {}", text, len);
    assert!(!unsafe { taken.val.boolval });
    assert_eq!(unsafe { v[0].val.intval }, 500);
    assert_eq!(unsafe { v[1].val.intval }, 766);
    assert_eq!(text, "v7");
    assert_eq!(len, 2);
    drop_value(taken, false);
    drop_value(v.pop().unwrap(), true); // c ("v7")
    drop_value(v.pop().unwrap(), false); // b (int 766)
    drop_value(v.pop().unwrap(), false); // int 500
    drop_value(s, true);
    drop_value(s2, true);
}
