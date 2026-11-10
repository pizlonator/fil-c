// Rc<RefCell<...>> graphs without cycles: parent/child links, borrow scopes,
// strong/weak counts, traversal.  All pointer words must survive the moves.
use std::cell::RefCell;
use std::rc::{Rc, Weak};

struct TreeNode {
    name: String,
    payload: Box<u64>,
    children: RefCell<Vec<Rc<TreeNode>>>,
    parent: RefCell<Weak<TreeNode>>,
}

impl TreeNode {
    fn new(name: &str, payload: u64) -> Rc<TreeNode> {
        Rc::new(TreeNode {
            name: name.to_string(),
            payload: Box::new(payload),
            children: RefCell::new(Vec::new()),
            parent: RefCell::new(Weak::new()),
        })
    }
}

fn main() {
    let root = TreeNode::new("root", 1);
    let left = TreeNode::new("left", 2);
    let right = TreeNode::new("right", 3);

    left.parent.borrow_mut().clone_from(&Rc::downgrade(&root));
    right.parent.borrow_mut().clone_from(&Rc::downgrade(&root));
    root.children.borrow_mut().push(left.clone());
    root.children.borrow_mut().push(right.clone());
    left.children.borrow_mut().push(TreeNode::new("left-child", 4));

    println!("root strong = {} weak = {}", Rc::strong_count(&root), Rc::weak_count(&root));
    println!("left strong = {}", Rc::strong_count(&left));

    let root_children = root.children.borrow();
    for child in root_children.iter() {
        let parent = child.parent.borrow().upgrade().map(|p| p.name.clone()).unwrap_or_default();
        let kids_borrow = child.children.borrow();
        let kids: Vec<&str> = kids_borrow.iter().map(|c| c.name.as_str()).collect();
        println!("child {} payload {} parent {} kids {}", child.name, child.payload, parent, kids.join(","));
    }

    // moving an Rc into a closure and back out of a Vec
    let moved: Vec<Rc<TreeNode>> = vec![left.clone(), right.clone()];
    let names: Vec<&str> = moved.iter().map(|t| t.name.as_str()).collect();
    println!("moved = {}", names.join(","));
    println!("left still strong = {}", Rc::strong_count(&left));

    // RefCell borrows: mutate a payload through a shared reference
    let mut_cell = Rc::new(RefCell::new(vec![Box::new("m1".to_string()), Box::new("m2".to_string())]));
    {
        let mut b = mut_cell.borrow_mut();
        b[0] = Box::new("M1".to_string());
        b.push(Box::new("m3".to_string()));
    }
    println!("mut = {}", mut_cell.borrow().iter().map(|b| b.as_str()).collect::<Vec<_>>().join(","));

    // Weak upgrade of a dropped parent
    let weak = Rc::downgrade(&right);
    drop(right);
    println!("weak alive after drop = {}", weak.upgrade().is_some());
    println!("root weak count = {}", Rc::weak_count(&root));
}
