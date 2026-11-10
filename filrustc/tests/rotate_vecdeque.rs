// VecDeque::rotate_left / make_contiguous: deque rotations run over element
// buffers holding pointer-containing values and then expose them as slices.
use std::collections::VecDeque;

fn main() {
    let mut dq: VecDeque<Box<String>> = VecDeque::new();
    for i in 0..8 {
        dq.push_back(Box::new(format!("v{}", i)));
    }
    for i in 0..3 {
        dq.push_front(Box::new(format!("f{}", i)));
    }
    dq.rotate_left(2);
    dq.make_contiguous();
    let joined: Vec<String> = dq.iter().map(|b| (**b).clone()).collect();
    println!("dq = {}", joined.join(" "));

    dq.rotate_right(4);
    let joined: Vec<String> = dq.iter().map(|b| (**b).clone()).collect();
    println!("after rr(4) = {}", joined.join(" "));

    // pop from both ends after rotation; values must still be usable
    let front = dq.pop_front().unwrap();
    let back = dq.pop_back().unwrap();
    println!("popped: {} / {}", front, back);
    println!("len = {}", dq.len());

    // wrap-around indexing still works
    dq.push_front(Box::new("wrap".to_string()));
    println!("idx0 = {}", dq[0]);
    println!("idxlast = {}", dq[dq.len() - 1]);
}
