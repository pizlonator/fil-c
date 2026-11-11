// Option/Result combinators: Option transpose/flatten/map_or/ok_or/and_then/
// or_else/filter/zip/iter/cloned/copied/inspect; Result transpose/flatten/
// map_or/and_then/or_else/map_err/inspect/iter/cloned; `?` chains inside a
// helper fn.
fn parse_it(s: &str) -> Result<i32, String> {
    let n: i32 = s.parse().map_err(|e: std::num::ParseIntError| format!("bad: {}", e))?;
    if n < 0 {
        Err("negative".to_string())
    } else {
        Ok(n)
    }
}

fn main() {
    // Option combinators.
    let some = Some(5);
    println!("map_or = {} {}", some.map_or(0, |x| x * 2), None::<i32>.map_or(0, |x| x * 2));
    println!("ok_or = {:?}", some.ok_or("no value"));
    println!("ok_or none = {:?}", None::<i32>.ok_or("no value"));
    println!(
        "and_then = {:?} {:?}",
        some.and_then(|x| if x > 3 { Some(x * 10) } else { None }),
        Some(2).and_then(|x| if x > 3 { Some(x * 10) } else { None })
    );
    println!(
        "or_else = {:?} {:?}",
        None::<i32>.or_else(|| Some(9)),
        Some(1).or_else(|| Some(9))
    );
    println!(
        "filter = {:?} {:?}",
        some.filter(|x| x % 2 == 1),
        Some(4).filter(|x| x % 2 == 1)
    );
    println!("zip = {:?} {:?} {:?}", some.zip(Some("z")), None::<i32>.zip(Some("z")), some.zip(None::<&str>));
    println!("iter = {:?}", some.iter().collect::<Vec<_>>());
    println!("iter none = {:?}", None::<i32>.iter().collect::<Vec<_>>());
    println!("cloned = {:?} copied = {:?}", Some(&5).cloned(), Some(&5).copied());
    let mut seen = 0;
    let _ = some.inspect(|x| seen = *x);
    println!("inspect = {}", seen);
    println!("transpose = {:?}", Some(Ok::<i32, &str>(7)).transpose());
    println!("transpose none = {:?}", None::<Result<i32, &str>>.transpose());
    println!("transpose err = {:?}", Some(Err::<i32, &str>("e")).transpose());
    println!("flatten = {:?}", Some(Some(6)).flatten());
    println!("flatten deep = {:?}", Some(Some(Some(3u8))).flatten());
    println!("flatten none = {:?}", Some(None::<u8>).flatten());
    println!("unwrap_or_else = {}", None::<i32>.unwrap_or_else(|| 42));

    // Result combinators (each constructed fresh: Result is consumed by
    // its combinators).
    println!("res map_or = {}", Ok::<i32, String>(4).map_or(0, |x| x + 1));
    let mut res_seen = 0;
    println!("res inspect = {}", Ok::<i32, String>(4).inspect(|x| res_seen = *x).unwrap());
    println!("res inspect seen = {}", res_seen);
    println!("res and_then = {:?}", Ok::<i32, String>(4).and_then(|x| Ok(x * 5)));
    println!(
        "res or_else = {:?} {:?}",
        Err::<i32, String>("boom".to_string()).or_else(|e| Ok::<i32, usize>(e.len() as i32)),
        Ok::<i32, String>(2).or_else(|e| Ok::<i32, usize>(e.len() as i32))
    );
    println!("res map_err = {:?}", Err::<i32, String>("boom".to_string()).map_err(|e| e.len()));
    println!("res map = {:?}", Ok::<i32, String>(4).map(|x| x + 1));
    println!("res iter = {:?}", Ok::<i32, &str>(12).iter().collect::<Vec<_>>());
    println!("res iter err = {:?}", Err::<i32, &str>("x").iter().collect::<Vec<_>>());
    println!("res cloned = {:?}", Ok::<&i32, &str>(&3).cloned());
    println!("res flatten = {:?}", Ok::<Result<u8, &str>, &str>(Ok(9)).flatten());
    println!("res flatten err = {:?}", Ok::<Result<u8, &str>, &str>(Err("deep")).flatten());
    println!("res transpose = {:?}", Ok::<Option<i32>, &str>(Some(8)).transpose());
    println!("res transpose none = {:?}", Ok::<Option<i32>, &str>(None).transpose());

    // `?` chains through the helper.
    println!("helper ok = {:?}", parse_it("42"));
    println!("helper bad = {:?}", parse_it("nope"));
    println!("helper neg = {:?}", parse_it("-3"));
}
