// Slice search: binary_search (hit/miss/head/tail), binary_search_by,
// binary_search_by_key, contains, starts_with/ends_with, sort +
// binary_search integration, first/last, and iter windows/chunks.
fn main() {
    let sorted = [2, 5, 8, 11, 15, 20, 27, 33];
    println!("hit = {:?}", sorted.binary_search(&15));
    println!("miss = {:?}", sorted.binary_search(&16));
    println!("head = {:?}", sorted.binary_search(&2));
    println!("tail = {:?}", sorted.binary_search(&33));
    println!("absent low = {:?}", sorted.binary_search(&1));
    println!("absent high = {:?}", sorted.binary_search(&40));

    // By and key: same slice, derived orderings.
    println!("by = {:?}", sorted.binary_search_by(|p| (*p * 3).cmp(&33))); // 11*3
    println!("by eq = {:?}", sorted.binary_search_by(|p| p.cmp(&8)));
    println!("key = {:?}", sorted.binary_search_by_key(&4, |p| *p / 5)); // 20/5
    println!("key miss = {:?}", sorted.binary_search_by_key(&7, |p| *p / 5));

    // contains / starts_with / ends_with on slices.
    let v = vec![33, 2, 27, 8, 15, 5, 20, 11];
    println!("contains = {} {} {}", v.contains(&15), v.contains(&99), sorted.contains(&27));
    let words = ["alpha", "beta", "gamma"];
    println!(
        "words starts = {} ends = {}",
        words[..].starts_with(&["alpha", "beta"]),
        words[..].ends_with(&["gamma"])
    );
    println!(
        "bytes starts = {} ends = {}",
        b"payload".as_slice().starts_with(b"pay"),
        b"payload".as_slice().ends_with(b"oad")
    );

    // sort + binary_search integration.
    let mut v = v;
    v.sort();
    println!("sorted = {:?}", v);
    println!("hit after sort = {:?}", v.binary_search(&27));
    println!("miss after sort = {:?}", v.binary_search(&6));
    v.sort_unstable();
    println!("still sorted = {}", v.windows(2).all(|w| w[0] <= w[1]));

    // first/last and iter window checks.
    println!("first = {:?} last = {:?}", sorted.first(), sorted.last());
    println!("empty first = {:?}", Vec::<u8>::new().first());
    let gaps: Vec<i32> = sorted.windows(2).map(|w| w[1] - w[0]).collect();
    println!("window gaps = {:?}", gaps);
    println!("chunks = {:?}", sorted.chunks(3).map(|c| c.len()).collect::<Vec<_>>());
    println!("iter sum = {}", sorted.iter().sum::<i32>());
}
