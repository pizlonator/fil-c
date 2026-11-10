// Safe MaybeUninit round-trips that move pointer-containing aggregates:
// new/assume_init, write + assume_init on arrays, MaybeUninit::read of a
// partially-initialized buffer guarded by ManuallyDrop.
use std::mem::{ManuallyDrop, MaybeUninit};

#[derive(Clone, Debug, PartialEq)]
struct Combo {
    name: String,
    ptr: Box<u32>,
    words: [u64; 2],
}

fn main() {
    // new + assume_init round trip
    let combo = Combo { name: "combo".into(), ptr: Box::new(7), words: [1, 2] };
    let mu = MaybeUninit::new(combo.clone());
    let back = unsafe { mu.assume_init() };
    println!("roundtrip = {} {} {}", back.name, back.ptr, back == combo);

    // uninit + write + assume_init for a struct with pointers
    let mut slot = MaybeUninit::<Combo>::uninit();
    slot.write(Combo { name: "written".into(), ptr: Box::new(88), words: [3, 4] });
    let written = unsafe { slot.assume_init() };
    println!("written = {} {}", written.name, written.ptr);

    // array of MaybeUninit<Box<String>>: fill then assume_init element-wise
    let mut arr: [MaybeUninit<Box<String>>; 4] = [const { MaybeUninit::uninit() }; 4];
    for (i, slot) in arr.iter_mut().enumerate() {
        slot.write(Box::new(format!("box{}", i)));
    }
    let filled: [Box<String>; 4] = arr.map(|slot| unsafe { slot.assume_init() });
    println!("array = {}", filled.iter().map(|b| b.as_str()).collect::<Vec<_>>().join(","));

    // ManuallyDrop-guarded read of a buffer where later slots are uninit
    let mut buf: [MaybeUninit<Combo>; 3] = [const { MaybeUninit::uninit() }; 3];
    buf[0].write(combo.clone());
    buf[1].write(Combo { name: "second".into(), ptr: Box::new(2), words: [5, 6] });
    let guard = ManuallyDrop::new(buf);
    let first = unsafe { guard[0].assume_init_ref() };
    let second = unsafe { guard[1].assume_init_ref() };
    println!("guarded = {} {}", first.name, second.name);

    // ManuallyDrop guarding a pointer-bearing struct: copy its fields out
    // explicitly, then forget the wrapper (leaking is fine for this test).
    let md = ManuallyDrop::new(Combo { name: "manual".into(), ptr: Box::new(5), words: [9, 9] });
    let mut copied = Combo { name: md.name.clone(), ptr: Box::new(*md.ptr), words: md.words };
    copied.name.push_str("+read");
    println!("manual = {}", copied.name);
}
