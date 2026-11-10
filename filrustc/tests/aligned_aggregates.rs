// Over-aligned (#[repr(align(16))] / #[repr(align(32))]) aggregates holding
// pointers: the typed-alloca path must keep capabilities for moves, swaps,
// Vec pushes/pops and by-value returns.
#[repr(align(16))]
struct A16 {
    name: String,
    boxed: Box<u32>,
    words: [u64; 2],
}

#[repr(align(32))]
struct A32 {
    inner: A16,
    more: Box<String>,
}

impl A16 {
    #[inline(never)]
    fn new(id: u32) -> A16 {
        A16 { name: format!("a{}", id), boxed: Box::new(id), words: [id as u64; 2] }
    }
}

impl A32 {
    #[inline(never)]
    fn new(id: u32) -> A32 {
        A32 { inner: A16::new(id), more: Box::new(format!("m{}", id)) }
    }
}

impl std::fmt::Display for A32 {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}:{}:{}:{}", self.inner.name, self.inner.boxed, self.inner.words[1], self.more)
    }
}

fn main() {
    println!("sizes {} {}", std::mem::size_of::<A16>(), std::mem::size_of::<A32>());
    println!("aligns {} {}", std::mem::align_of::<A16>(), std::mem::align_of::<A32>());

    let mut v = vec![A32::new(1), A32::new(2), A32::new(3)];
    {
        let (l, r) = v.split_at_mut(2);
        std::mem::swap(&mut l[0], &mut r[0]);
    }
    for x in &v {
        println!("item = {}", x);
    }

    let taken = v.pop().unwrap();
    println!("popped = {}", taken);

    // by-value moves through an inline(never) call chain
    #[inline(never)]
    fn pass_through(mut x: A32) -> A32 {
        x.inner.words[1] += 1000;
        x
    }
    let passed = pass_through(taken);
    println!("passed = {}", passed);

    // fixed-size array of over-aligned aggregates
    let arr = [A32::new(4), A32::new(5)];
    for x in &arr {
        println!("arr = {}", x);
    }
}
