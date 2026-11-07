fn main() {
    let mut v: Vec<i32> = Vec::new();
    v.push(40);
    v.push(2);
    println!("no-prepopulate sum = {}", v.iter().sum::<i32>());
}
