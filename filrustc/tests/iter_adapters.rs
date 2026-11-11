use std::collections::{HashMap, VecDeque};

fn main() {
    let v1 = vec![1i32, 2, 3];
    let v2 = vec![4i32, 5, 6];
    let chained: Vec<i32> = v1.iter().chain(&v2).cloned().collect();
    println!("chain = {:?}", chained);
    let zipped: Vec<String> =
        v1.iter().zip(v2.iter()).map(|(a, b)| format!("{}:{}", a, b)).collect();
    println!("zip = {}", zipped.join(","));
    println!("rev = {:?}", v1.iter().rev().collect::<Vec<_>>());

    let scanned: Vec<i32> = v1.iter().scan(0, |acc, x| { *acc += x; Some(*acc) }).collect();
    println!("scan = {:?}", scanned);
    let folded = v1.iter().fold(10, |acc, x| acc + x);
    println!("fold = {}", folded);
    let flat: Vec<i32> = vec![vec![1, 2], vec![3, 4]].into_iter().flat_map(|v| v).collect();
    println!("flat_map = {:?}", flat);

    let mut peek = v1.iter().peekable();
    let peeked = *peek.peek().unwrap();
    let first = *peek.next().unwrap();
    println!("peeked = {} next = {}", peeked, first);
    let take_while: Vec<i32> = v1.iter().copied().take_while(|x| *x < 3).collect();
    println!("take_while = {:?}", take_while);
    let skip_while: Vec<i32> = v1.iter().copied().skip_while(|x| *x < 2).collect();
    println!("skip_while = {:?}", skip_while);
    let step: Vec<i32> = (0..10).step_by(3).collect();
    println!("step_by = {:?}", step);
    let (evens, odds): (Vec<i32>, Vec<i32>) = (0..6).partition(|x| x % 2 == 0);
    println!("partition = {:?} {:?}", evens, odds);

    let words = vec!["alpha", "beta"];
    let interspersed: String = words.iter().copied().collect::<Vec<_>>().join(", ");
    println!("joined = {}", interspersed);
    let mapped: HashMap<String, usize> =
        words.iter().enumerate().map(|(i, w)| (w.to_string(), i)).collect();
    println!("map = {} {}", mapped["alpha"], mapped["beta"]);
    let deque: VecDeque<Box<String>> =
        words.iter().map(|w| Box::new(w.to_string())).collect();
    println!("deque = {}", deque.iter().map(|b| b.as_str()).collect::<Vec<_>>().join(","));

    let position = v1.iter().position(|x| *x == 2);
    println!("position = {:?}", position);
    let find_map = v1.iter().find_map(|x| if *x > 1 { Some(x * 10) } else { None });
    println!("find_map = {:?}", find_map);
    let max_by = v1.iter().max_by_key(|x| -(**x));
    println!("max_by = {:?}", max_by);
    let sum: i32 = v1.iter().sum();
    let product: i32 = v1.iter().product();
    println!("sum = {} product = {}", sum, product);

    let try_fold: Result<i32, &str> = v1.iter().try_fold(0, |acc, x| if *x < 5 { Ok(acc + x) } else { Err("too big") });
    println!("try_fold = {:?}", try_fold);

    let mut counter = 0u64;
    let from_fn: Vec<u64> =
        std::iter::from_fn(|| { let n = counter; counter += 1; (n < 4).then(|| n * 11) }).collect();
    println!("from_fn = {:?}", from_fn);
    let successors: Vec<u64> = std::iter::successors(Some(1u64), |n| (*n < 8).then(|| n * 2)).collect();
    println!("successors = {:?}", successors);
    let solo = std::iter::once("solo".to_string()).next().unwrap();
    let repeated: Vec<String> = std::iter::repeat("rep".to_string()).take(3).collect();
    println!("once+rep = {} {}", solo, repeated.join(","));
    let by_ref = v1.iter().by_ref().take(2).cloned().collect::<Vec<_>>();
    println!("by_ref = {:?}", by_ref);
    let filter_map: Vec<String> =
        ["1", "x", "3"].iter().filter_map(|s| s.parse::<i32>().ok().map(|n| format!("n{}", n * 2))).collect();
    println!("filter_map = {:?}", filter_map);
}
