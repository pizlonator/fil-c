// [LargeStruct; N] moves: indexing, swapping, into_iter round-trips,
// each_ref / each_mut / map on arrays of pointer-containing values.
use std::mem::swap;

#[derive(Clone)]
struct Node {
    name: String,
    boxed: Box<u32>,
}

impl Node {
    #[inline(never)]
    fn new(id: u32) -> Node {
        Node { name: format!("n{}", id), boxed: Box::new(id * 100) }
    }
}

fn main() {
    // array of 3 pointer-containing structs, created inline (stack allocas)
    let mut arr = [Node::new(1), Node::new(2), Node::new(3)];
    println!("arr = {} {} {}", arr[0].name, arr[1].name, arr[2].name);

    // move the whole array into a function and back out (by-value aggregate move)
    #[inline(never)]
    fn transform(mut a: [Node; 3]) -> [Node; 3] {
        {
            let (l, r) = a.split_at_mut(2);
            swap(&mut l[0], &mut r[0]);
        }
        a[1].boxed = Box::new(999);
        a
    }
    let mut arr = transform(arr);
    println!("after = {} {} {}", arr[0].name, arr[1].boxed, arr[2].name);

    // each_ref / each_mut
    for node in arr.each_mut() {
        node.name.push('!');
    }
    let names: Vec<&str> = arr.each_ref().iter().map(|n| n.name.as_str()).collect();
    println!("each = {}", names.join(","));

    // array map producing a new array (moves elements)
    let boxed_arr: [Box<String>; 3] = arr.map(|n| Box::new(n.name));
    println!("map = {}", boxed_arr.iter().map(|b| b.as_str()).collect::<Vec<_>>().join(","));

    // into_iter round-trip
    let collected: Vec<Box<String>> = boxed_arr.into_iter().collect();
    println!("into = {}", collected.iter().map(|b| b.as_str()).collect::<Vec<_>>().join(","));

    // array of 8-byte pointer elements
    let mut pointers: [Option<Box<u64>>; 4] = [Some(Box::new(10)), Some(Box::new(20)), None, Some(Box::new(40))];
    {
        let (l, r) = pointers.split_at_mut(3);
        swap(&mut l[0], &mut r[0]);
    }
    let vals: Vec<String> = pointers.iter().map(|p| p.as_ref().map(|b| b.to_string()).unwrap_or("-".into())).collect();
    println!("pointers = {}", vals.join(","));

    // 2D array of pointer elements
    let grid: [[Box<String>; 2]; 2] = [
        [Box::new("r0c0".into()), Box::new("r0c1".into())],
        [Box::new("r1c0".into()), Box::new("r1c1".into())],
    ];
    println!("grid = {} {} {} {}", grid[0][0], grid[0][1], grid[1][0], grid[1][1]);
    let flat: Vec<&str> = grid.iter().flatten().map(|b| b.as_str()).collect();
    println!("flat = {}", flat.join(","));
}
