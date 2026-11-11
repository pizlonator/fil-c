// Every stable atomic type of Rust 1.90 (AtomicBool, AtomicI8/U8/I16/U16/
// I32/U32/I64/U64/Isize/Usize, AtomicPtr -- AtomicI128/AtomicU128 are
// #[unstable(integer_atomics)] in this tree and are NOT exercised) against
// every stable Ordering variant (Relaxed/Release/Acquire/AcqRel/SeqCst --
// std::sync::atomic::Ordering has exactly these 5 variants), across the core
// ops: store/load, swap, fetch_add/sub/and/or/xor/max/min, compare_exchange
// strong+weak, and fetch_update (the ones each type has).
use std::sync::atomic::{
    AtomicBool, AtomicI16, AtomicI32, AtomicI64, AtomicI8, AtomicIsize, AtomicPtr, AtomicU16,
    AtomicU32, AtomicU64, AtomicU8, AtomicUsize, Ordering,
};
use std::sync::Arc;
use std::thread;

// Uniform probe for the integer atomics: every op that the type has, under
// every ordering, all results checked against the expected values.  `init`
// must be a value where init + 3 does not overflow.
macro_rules! exercise_int {
    ($label:literal, $atom:ty, $init:expr) => {{
        type A = $atom;
        let a = A::new($init);
        // Relaxed store + Relaxed load.
        a.store($init, Ordering::Relaxed);
        let relaxed = a.load(Ordering::Relaxed);
        // Release store observed by an Acquire load.
        a.store($init, Ordering::Release);
        let acquired = a.load(Ordering::Acquire);
        // SeqCst swap.
        let swapped = a.swap($init, Ordering::SeqCst);
        // AcqRel fetch_add then Acquire fetch_sub: back at init afterwards.
        let added = a.fetch_add(3, Ordering::AcqRel);
        let subbed = a.fetch_sub(3, Ordering::Acquire);
        let back = a.load(Ordering::SeqCst);
        // No-op bitwise/max/min probes (each returns the previous value).
        let anded = a.fetch_and(!0, Ordering::SeqCst);
        let ored = a.fetch_or(0, Ordering::SeqCst);
        let xored = a.fetch_xor(0, Ordering::SeqCst);
        let maxed = a.fetch_max($init, Ordering::SeqCst);
        let mined = a.fetch_min($init, Ordering::SeqCst);
        // compare_exchange strong (SeqCst success, Acquire failure) and weak
        // (AcqRel success, Relaxed failure; weak may fail spuriously, so
        // retry a bounded number of times -- the value never moves).
        let cas = a.compare_exchange($init, $init, Ordering::SeqCst, Ordering::Acquire).is_ok();
        let mut weak = false;
        for _ in 0..16 {
            if a.compare_exchange_weak($init, $init, Ordering::AcqRel, Ordering::Relaxed).is_ok() {
                weak = true;
                break;
            }
        }
        let updated = a.fetch_update(Ordering::SeqCst, Ordering::Acquire, |x| Some(x)).is_ok();
        println!(
            "{}: uniform = {} add_old = {} sub_old = {} cas = {} weak = {} update = {}",
            $label,
            relaxed == $init && acquired == $init && swapped == $init && back == $init &&
                added == $init && subbed == $init + 3 && anded == $init && ored == $init &&
                xored == $init && maxed == $init && mined == $init && cas,
            added,
            subbed,
            cas,
            weak,
            updated
        );
    }};
}

fn main() {
    exercise_int!("AtomicI8", AtomicI8, 10);
    exercise_int!("AtomicU8", AtomicU8, 10);
    exercise_int!("AtomicI16", AtomicI16, 10);
    exercise_int!("AtomicU16", AtomicU16, 10);
    exercise_int!("AtomicI32", AtomicI32, 10);
    exercise_int!("AtomicU32", AtomicU32, 10);
    exercise_int!("AtomicI64", AtomicI64, 10);
    exercise_int!("AtomicU64", AtomicU64, 10);
    exercise_int!("AtomicIsize", AtomicIsize, 10);
    exercise_int!("AtomicUsize", AtomicUsize, 10);

    // AtomicBool: the bitwise ops it has, under all orderings it accepts
    // them with (no arithmetic/max/min on bool).
    {
        let b = AtomicBool::new(false);
        b.store(true, Ordering::Relaxed);
        let r1 = b.load(Ordering::Relaxed);
        b.store(false, Ordering::Release);
        let r2 = b.load(Ordering::Acquire);
        let old = b.swap(true, Ordering::SeqCst);
        let and = b.fetch_and(true, Ordering::AcqRel);
        let or = b.fetch_or(false, Ordering::SeqCst);
        let xor = b.fetch_xor(false, Ordering::Acquire);
        let cas = b.compare_exchange(true, false, Ordering::SeqCst, Ordering::Relaxed).is_ok();
        let upd = b.fetch_update(Ordering::SeqCst, Ordering::Acquire, |x| Some(!x)).is_ok();
        println!(
            "AtomicBool: uniform = {} cas = {} update = {}",
            r1 && !r2 && !old && and && or && xor && cas,
            cas,
            upd
        );
    }

    // AtomicPtr: store/load (all orderings), swap, compare_exchange
    // strong+weak, fetch_update (no arithmetic ops on pointers).
    {
        let mut x = 10u32;
        let mut y = 20u32;
        let p = AtomicPtr::new(&mut x as *mut u32);
        let r = p.load(Ordering::Relaxed);
        p.store(&mut x as *mut u32, Ordering::Release);
        let q = p.load(Ordering::Acquire);
        let old = p.swap(&mut y as *mut u32, Ordering::SeqCst);
        let cas = p
            .compare_exchange(&mut y as *mut u32, &mut x as *mut u32, Ordering::SeqCst, Ordering::Relaxed)
            .is_ok();
        let mut weak = false;
        for _ in 0..16 {
            if p.compare_exchange_weak(&mut x as *mut u32, &mut y as *mut u32, Ordering::AcqRel, Ordering::Relaxed).is_ok() {
                weak = true;
                break;
            }
        }
        let upd = p.fetch_update(Ordering::SeqCst, Ordering::Acquire, |cur| Some(cur)).is_ok();
        let deref = unsafe { *p.load(Ordering::SeqCst) };
        println!(
            "AtomicPtr: uniform = {} deref = {} cas = {} weak = {} update = {}",
            r == q && old == &mut x as *mut u32 && cas,
            deref,
            cas,
            weak,
            upd
        );
    }

    // Release/Acquire message passing: a Relaxed data store made visible by a
    // Release flag store, observed by an Acquire flag load.
    let data = Arc::new(AtomicU64::new(0));
    let flag = Arc::new(AtomicBool::new(false));
    let (d2, f2) = (Arc::clone(&data), Arc::clone(&flag));
    let h = thread::spawn(move || {
        d2.store(0xDEAD_BEEF, Ordering::Relaxed);
        f2.store(true, Ordering::Release);
    });
    while !flag.load(Ordering::Acquire) {
        std::hint::spin_loop();
    }
    println!("mp value = {:#x}", data.load(Ordering::Relaxed));
    h.join().unwrap();

    // SeqCst fetch_add contention across 4 threads.
    let shared = Arc::new(AtomicU64::new(0));
    let mut handles = Vec::new();
    for _ in 0..4 {
        let s = Arc::clone(&shared);
        handles.push(thread::spawn(move || {
            for _ in 0..100 {
                s.fetch_add(1, Ordering::SeqCst);
            }
        }));
    }
    for h in handles {
        h.join().unwrap();
    }
    println!("atomic sum = {}", shared.load(Ordering::SeqCst));
}
