// slice::concat / join / connect of pointer-containing slices: these memmove
// whole elements into fresh heap allocations.
fn main() {
    let parts: Vec<Vec<Box<String>>> = vec![
        vec![Box::new("c1".into()), Box::new("c2".into())],
        vec![Box::new("c3".into())],
        vec![Box::new("c4".into()), Box::new("c5".into())],
    ];
    let flattened: Vec<Box<String>> = parts.clone().into_iter().flatten().collect();
    println!("flattened = {}", flattened.iter().map(|b| b.as_str()).collect::<Vec<_>>().join(","));

    // Vec<Box<str>> concat
    let strs: Vec<Box<str>> = vec!["s1".into(), "s2".into(), "s3".into()];
    let joined: String = strs.iter().map(|s| &**s).collect::<Vec<_>>().join("-");
    println!("joined = {}", joined);

    // concat of Vec<Vec<String>>
    let vvs: Vec<Vec<String>> = vec![vec!["a".into(), "b".into()], vec!["c".into()]];
    let concatenated: Vec<String> = vvs.concat();
    println!("concat = {}", concatenated.join("|"));

    // clone a Vec of Boxes (deep clones through Clone, not memcpys)
    let cloned = flattened.clone();
    println!("cloned = {} same-ptr = {}", cloned.iter().map(|b| b.as_str()).collect::<Vec<_>>().join(","),
        cloned[0].as_ptr() == flattened[0].as_ptr());

    // repeat a Box<str> into a Vec and join
    let repeated: Vec<Box<str>> = (0..3).map(|_| "rp".into()).collect();
    println!("repeated = {}", repeated.iter().map(|s| &**s).collect::<Vec<_>>().join(""));

    // sort strings that share prefixes (string compare + moves)
    let mut names: Vec<Box<str>> = vec!["bb".into(), "ba".into(), "ab".into(), "aa".into()];
    names.sort();
    println!("names = {}", names.iter().map(|s| &**s).collect::<Vec<_>>().join(","));
}
