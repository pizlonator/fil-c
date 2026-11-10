// mem::swap family on LARGE pointer-containing aggregates (> 32 bytes) and
// on arrays of them: the regime where swap_nonoverlapping's pointer-typed
// chunk loop (std change #22) does many pointer-word chunk swaps.
use std::mem::swap;

#[inline(never)]
fn make(id: u32, tag: &str) -> Big {
    let mut raw = Box::into_raw(Box::new(id));
    if id == 4 {
        // a second raw pointer, so the struct holds two distinct capabilities
        raw = Box::into_raw(Box::new(id * 100));
    }
    Big {
        name: tag.to_string(),
        ptr: raw as *mut u8,
        id,
        words: [id as u64, id as u64 + 1, id as u64 + 2, id as u64 + 3],
    }
}

struct Big {
    name: String,
    ptr: *mut u8,
    id: u32,
    words: [u64; 4],
}

impl Drop for Big {
    fn drop(&mut self) {
        if !self.ptr.is_null() {
            unsafe { drop(Box::from_raw(self.ptr as *mut u32)); }
        }
    }
}

impl std::fmt::Display for Big {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let deref = unsafe { *(self.ptr as *const u32) };
        write!(f, "{}#{}*{}[{}]", self.name, self.id, deref, self.words.map(|w| w.to_string()).join(","))
    }
}

#[inline(never)]
fn dump(items: &[Big]) -> String {
    items.iter().map(|b| b.to_string()).collect::<Vec<_>>().join(" | ")
}

fn main() {
    println!("size = {}", std::mem::size_of::<Big>());

    // swap two big locals
    let mut a = make(1, "aa");
    let mut b = make(2, "bb");
    swap(&mut a, &mut b);
    println!("pair: {} {}", a, b);

    // swap within an array of big structs (stack allocas)
    let mut arr = [make(3, "cc"), make(4, "dd"), make(5, "ee")];
    {
        let (l, r) = arr.split_at_mut(2);
        swap(&mut l[0], &mut r[0]);
    }
    println!("array0: {}", arr[0]);
    println!("array2: {}", arr[2]);

    // swap through references into a Vec
    let mut v = vec![make(6, "ff"), make(7, "gg"), make(8, "hh")];
    {
        let (l, r) = v.split_at_mut(1);
        swap(&mut l[0], &mut r[1]);
    }
    println!("vec: {}", dump(&v));

    // a swap-heavy shuffle; contents must survive bit-for-bit
    let mut w = vec![make(9, "ii"), make(10, "jj"), make(11, "kk"), make(12, "ll")];
    for i in 0..4 {
        for j in 0..3 {
            let (l, r) = w.split_at_mut(j + 1);
            swap(&mut l[j], &mut r[0]);
        }
    }
    println!("shuffled: {}", dump(&w));
}
