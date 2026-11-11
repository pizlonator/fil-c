use std::mem::{self, discriminant, size_of, align_of};
use std::num::{NonZeroI32, NonZeroI64, NonZeroI8, NonZeroU16, NonZeroU32, NonZeroU64, NonZeroU8, NonZeroUsize};
use std::future::Future;
use std::pin::Pin;

#[derive(Debug)]
enum Shape { Circle(u64), Square(Box<String>), Point }

// Fil-C: `Pin<Box<dyn Future>>::as_mut().poll()` needs a named helper so the
// poll call site has an `Unpin`-free path through a concrete self type.
struct PinFutureHelper(Pin<Box<dyn std::future::Future<Output = u32>>>);
impl PinFutureHelper {
    fn poll(&mut self, cx: &mut std::task::Context<'_>) -> std::task::Poll<u32> {
        self.0.as_mut().poll(cx)
    }
}

fn main() {
    // mem::discriminant
    let c = Shape::Circle(5);
    let s = Shape::Square(Box::new("sq".to_string()));
    let p = Shape::Point;
    println!("discriminant distinct = {}", discriminant(&c) != discriminant(&s) && discriminant(&s) != discriminant(&p));
    println!("discriminant same = {}", discriminant(&c) == discriminant(&Shape::Circle(9)));

    // size_of / align_of
    println!("sizes = {} {} {} {}", size_of::<u64>(), size_of::<String>(), size_of::<Option<&u8>>(), size_of::<Shape>());
    println!("aligns = {} {} {}", align_of::<u64>(), align_of::<String>(), align_of::<Shape>());
    println!("variant sizes = {} {} {}", size_of::<NonZeroU8>(), size_of::<NonZeroU64>(), size_of::<NonZeroUsize>());

    // mem::swap/take/replace on aggregates
    let mut x = Shape::Square(Box::new("x".to_string()));
    let mut y = Shape::Circle(42);
    mem::swap(&mut x, &mut y);
    match (&x, &y) {
        (Shape::Circle(n), Shape::Square(s)) => println!("swapped = {} {}", n, s),
        _ => println!("unexpected"),
    }
    let taken = mem::replace(&mut y, Shape::Point);
    match taken {
        Shape::Square(s) => println!("taken = {}", s),
        _ => println!("unexpected take"),
    }

    // NonZero types
    let nz8 = NonZeroU8::new(255).unwrap();
    let nz16 = NonZeroU16::new(65535).unwrap();
    let nz32 = NonZeroU32::new(4000000000).unwrap();
    let nz64 = NonZeroU64::new(u64::MAX).unwrap();
    let nzi32 = NonZeroI32::new(-123456).unwrap();
    let nzi64 = NonZeroI64::new(i64::MIN).unwrap();
    let nzi8 = NonZeroI8::new(-128).unwrap();
    let nzusize = NonZeroUsize::new(usize::MAX).unwrap();
    println!("nonzero = {} {} {} {}", nz8.get(), nz16.get(), nz32.get(), nz64.get());
    println!("nonzero signed = {} {} {}", nzi8.get(), nzi32.get(), nzi64.get());
    println!("nonzero usize = {}", nzusize.get());
    println!("nonzero none = {} {}", NonZeroU32::new(0).is_none(), NonZeroI8::new(0).is_none());
    println!("nonzero get_signed = {} {}", nzi32.abs().get(), nz32.get() as u64);
    println!("nonzeroLeadingZeros = {}", nz64.leading_zeros());

    // Pin basics
    let mut boxed = Box::new(5u64);
    let mut pinned: Pin<&mut u64> = Pin::new(&mut boxed);
    println!("pinned get_ref = {}", *pinned);
    *pinned = 7;
    println!("pinned mutate = {}", *pinned);

    let pinned_box: Pin<Box<String>> = Box::pin("pinned-string".to_string());
    println!("pinned box = {} len = {}", pinned_box.as_ref().get_ref(), pinned_box.len());

    // Pin<Box<dyn Future>> shape
    let fut: Pin<Box<dyn std::future::Future<Output = u32>>> = Box::pin(async { 33 });
    let waker = std::task::Waker::noop();
    let mut cx = std::task::Context::from_waker(waker);
    let out = PinFutureHelper(fut).poll(&mut cx);
    println!("pinned future = {:?}", out.is_ready());
}
