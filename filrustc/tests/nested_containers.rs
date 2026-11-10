// Nested pointer-bearing containers: Vec<Vec<String>>,
// Vec<Option<Box<Node>>>, structs of structs with pointers and words.
#[derive(Clone)]
struct Leaf {
    value: String,
    ptr: Box<u32>,
}

#[derive(Clone)]
struct Branch {
    leaves: Vec<Leaf>,
    words: [u64; 3],
}

fn main() {
    // Vec<Vec<String>>
    let vv: Vec<Vec<String>> = vec![
        vec!["a".into(), "b".into()],
        vec!["c".into()],
        vec!["d".into(), "e".into(), "f".into()],
    ];
    let total: usize = vv.iter().map(|v| v.len()).sum();
    println!("vv total = {} mid = {}", total, vv[2][1]);

    // deep clone then mutate one path
    let mut cloned = vv.clone();
    cloned[1][0] = "C".into();
    println!("clone paths = {}", cloned.iter().map(|v| v.join("")).collect::<Vec<_>>().join("/"));

    // Vec<Option<Box<Node>>>
    struct Node {
        name: String,
        inner: Leaf,
    }
    let mut nodes: Vec<Option<Box<Node>>> = Vec::new();
    for i in 0..4 {
        if i == 2 {
            nodes.push(None);
        } else {
            nodes.push(Some(Box::new(Node {
                name: format!("node{}", i),
                inner: Leaf { value: format!("leaf{}", i), ptr: Box::new(i * 11) },
            })));
        }
    }
    for (i, n) in nodes.iter().enumerate() {
        match n {
            Some(nx) => println!("{} = {} {} {}", i, nx.name, nx.inner.value, nx.inner.ptr),
            None => println!("{} = none", i),
        }
    }

    // struct-in-struct with pointers and words, cloned and compared
    let b1 = Branch {
        leaves: vec![Leaf { value: "L1".into(), ptr: Box::new(1) }, Leaf { value: "L2".into(), ptr: Box::new(2) }],
        words: [7, 8, 9],
    };
    let b2 = b1.clone();
    let same = b1.leaves.len() == b2.leaves.len()
        && b1.leaves.iter().zip(b2.leaves.iter()).all(|(x, y)| x.value == y.value && x.ptr == y.ptr)
        && b1.words == b2.words;
    println!("branch clone same = {}", same);
    println!("branch = {} {}", b2.leaves[1].value, b2.words[2]);

    // move a branch into a function and back
    #[inline(never)]
    fn grow(mut b: Branch) -> Branch {
        b.leaves.push(Leaf { value: "L3".into(), ptr: Box::new(3) });
        b.words[0] += 100;
        b
    }
    let b3 = grow(b2);
    println!("grown = {} {} {}", b3.leaves.iter().map(|l| l.value.clone()).collect::<Vec<_>>().join(","), b3.words[0], b3.leaves[2].ptr);

    // Option<Branch> take / replace
    let mut opt = Some(b3);
    let prev = std::mem::replace(&mut opt, None);
    println!("opt none = {} prev leaves = {}", opt.is_none(), prev.unwrap().leaves.len());
}
