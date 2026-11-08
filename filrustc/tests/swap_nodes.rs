#[derive(Clone)]
struct Node { name: String, next: *mut u8, id: u32 }
static mut CELLS: [u8; 4] = [10, 20, 30, 40];
#[inline(never)] fn make(i: usize) -> Node { Node { name: format!("node{}", i),
    next: unsafe { std::ptr::addr_of_mut!(CELLS) as *mut u8 }.wrapping_add(i % 4), id: (i * 7) as u32 } }
#[inline(never)] fn read(n: &Node) -> u8 { unsafe { *n.next } }
#[inline(never)] fn shuffle(a: &mut Node, b: &mut Node) { std::mem::swap(a, b); }
fn main() {
    let mut nodes: Vec<Node> = (0..16).map(make).collect();
    for i in 0..8 { let (l, r) = nodes.split_at_mut(8); shuffle(&mut l[i], &mut r[i]); }
    let vals: Vec<u8> = nodes.clone().iter().map(read).collect();
    println!("vals = {:?}", vals);
}
