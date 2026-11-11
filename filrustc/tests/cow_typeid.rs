// Cow (borrowed stays borrowed, to_mut materializes, into_owned produces an
// owned value, Cow<'a, str> and Cow<'a, [T]> patterns) plus TypeId::of and
// Any::is/downcast_ref/downcast/type_id over a few concrete types.
use std::any::{Any, TypeId};
use std::borrow::Cow;

fn describe(v: &dyn Any) -> &'static str {
    if v.is::<u32>() {
        "u32"
    } else if v.is::<String>() {
        "String"
    } else if v.is::<Vec<u64>>() {
        "Vec<u64>"
    } else {
        "other"
    }
}

fn main() {
    let borrowed: Cow<'_, str> = Cow::Borrowed("lends");
    println!("borrowed = {}", borrowed);
    let mut cow: Cow<'_, str> = Cow::from("static");
    println!("from str = {}", cow);
    cow.to_mut().push_str(" + added");
    println!("after to_mut = {}", cow);
    let cow2: Cow<'_, str> = Cow::from(String::from("owned base"));
    println!("from string = {}", cow2);
    let taken: String = cow2.into_owned();
    println!("into_owned = {}", taken);
    let via_clone: Cow<'_, str> = Cow::Owned(taken.clone());
    println!("owned cow = {}", via_clone);
    let back = via_clone.into_owned();
    println!("owned cow into = {}", back);

    // Cow over a slice: borrowed first, grown via to_mut.
    let nums = [1u32, 2, 3];
    let cow_nums: Cow<'_, [u32]> = Cow::Borrowed(&nums[..]);
    println!("cow slice = {:?}", &cow_nums[..]);
    let mut cow_grown: Cow<'_, [u32]> = Cow::Borrowed(&nums[..]);
    cow_grown.to_mut().push(4);
    println!("cow slice grown = {:?}", &cow_grown[..]);

    // TypeId of concrete types.
    println!(
        "typeid = {} {} {}",
        TypeId::of::<u32>() == TypeId::of::<u32>(),
        TypeId::of::<u32>() != TypeId::of::<u64>(),
        TypeId::of::<u32>() != TypeId::of::<i32>()
    );

    // Any: is / downcast_ref / downcast_mut / downcast / type_id on dyn Any.
    let boxed: Box<dyn Any> = Box::new(41u32);
    println!("is = {} {}", boxed.is::<u32>(), !boxed.is::<&str>());
    println!("downcast_ref = {}", boxed.downcast_ref::<u32>().copied().unwrap());
    println!("describe = {}", describe(boxed.as_ref()));
    let mut boxed_mut: Box<dyn Any> = Box::new(1u64);
    if let Some(v) = boxed_mut.downcast_mut::<u64>() {
        *v = 99;
    }
    println!("downcast_mut = {}", boxed_mut.downcast_ref::<u64>().copied().unwrap());
    let boxed_str: Box<dyn Any> = Box::new("text".to_string());
    println!("downcast str = {}", boxed_str.downcast_ref::<String>().unwrap());
    println!("describe str = {}", describe(boxed_str.as_ref()));
    // type_id on the *dyn Any itself* (not the Box's own Any impl) matches
    // the static TypeId.
    println!("type_id match = {}", (*boxed_str).type_id() == TypeId::of::<String>());
    let boxed_vec: Box<dyn Any> = Box::new(vec![7u64, 8, 9]);
    println!("describe vec = {}", describe(boxed_vec.as_ref()));
    let v = boxed_vec.downcast::<Vec<u64>>().unwrap();
    println!("downcast owned = {:?}", v);
    // Any over references.
    let payload = 5u8;
    let any_ref: &dyn Any = &payload;
    println!("any ref = {}", any_ref.downcast_ref::<u8>().copied().unwrap());
}
