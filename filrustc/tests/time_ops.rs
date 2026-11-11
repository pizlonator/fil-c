use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

fn main() {
    let start = Instant::now();
    std::thread::sleep(Duration::from_millis(15));
    let elapsed = start.elapsed();
    println!("elapsed >= 15ms = {}", elapsed >= Duration::from_millis(15));
    println!("elapsed < 5s = {}", elapsed < Duration::from_secs(5));
    println!("instant add = {}", (start + Duration::from_secs(1)) > start);

    let d = Duration::from_secs(61) + Duration::from_millis(1500);
    println!("as_secs = {} subsec = {}", d.as_secs(), d.subsec_millis());
    println!("mul = {:?} div = {:?}", Duration::from_secs(2) * 3, Duration::from_secs(9) / 2);
    println!("checked = {:?} {:?}", Duration::from_secs(2).checked_mul(3), Duration::MAX.checked_add(Duration::from_secs(1)));
    println!("saturating = {:?}", Duration::from_secs(1).saturating_sub(Duration::from_secs(2)));
    println!("as_millis = {} as_micros = {} as_nanos = {}", d.as_millis(), d.as_micros(), d.as_nanos());

    let now = SystemTime::now();
    let since_epoch = now.duration_since(UNIX_EPOCH).unwrap();
    println!("epoch = {}", since_epoch.as_secs() > 1_600_000_000);
    let before = SystemTime::UNIX_EPOCH;
    println!("before epoch err = {}", now.duration_since(before).is_ok());
    let past = UNIX_EPOCH - Duration::from_secs(100);
    println!("negative = {}", past < UNIX_EPOCH);
    println!("checked_add = {:?} overflow = {:?}", now.checked_add(Duration::from_secs(100)).is_some(), SystemTime::UNIX_EPOCH.checked_add(Duration::from_secs(u64::MAX)).is_none());
    println!("sys elapsed small = {}", now.elapsed().map(|d| d.as_secs() < 60).unwrap_or(false));

    let instant_arith = Instant::now();
    let later = instant_arith + Duration::from_millis(10);
    println!("instant sub = {}", (later - instant_arith).as_millis() >= 10);
    println!("duration fmt = {} secs {} ms", Duration::from_secs(1).as_secs_f64(), Duration::from_millis(1500).as_secs_f32());
    let zero = Duration::ZERO;
    println!("zero = {} default = {:?}", zero == Duration::new(0, 0), zero < Duration::from_nanos(1));
    println!("from micros = {:?}", Duration::from_micros(1500));
}
