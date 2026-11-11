// std::os::fd on File: AsRawFd/AsFd/BorrowedFd, FromRawFd/IntoRawFd (dup round
// trip), OwnedFd (File -> OwnedFd -> File), and try_clone interplay (a dup'd
// descriptor shares the open file description, so writes through one handle
// are visible and the cursor is shared).
use std::fs::{File, OpenOptions};
use std::io::{prelude::*, Seek, SeekFrom};
use std::os::fd::{AsFd, AsRawFd, BorrowedFd, FromRawFd, IntoRawFd, OwnedFd};

fn main() {
    let dir = format!("/tmp/filc-osfd-{}", std::process::id());
    std::fs::create_dir_all(&dir).unwrap();
    let path = format!("{}/fd.txt", dir);

    // Read+write (File::create alone would be O_WRONLY, and reads through it
    // -- or through any dup of it -- are legitimately EBADF).
    let mut f = OpenOptions::new()
        .read(true)
        .write(true)
        .create(true)
        .truncate(true)
        .open(&path)
        .unwrap();
    f.write_all(b"fd round trip").unwrap();
    println!("created = {}", std::fs::metadata(&path).unwrap().len() == 13);

    // The same underlying descriptor through three views.
    let raw = f.as_raw_fd();
    let borrowed: BorrowedFd<'_> = f.as_fd();
    println!("borrowed matches = {}", borrowed.as_raw_fd() == raw);

    // try_clone dups the descriptor: a different fd number, the same open
    // file description (shared cursor).
    let mut dup = f.try_clone().unwrap();
    println!("dup fd differs = {}", dup.as_raw_fd() != raw);
    writeln!(dup, " via dup").unwrap();
    dup.seek(SeekFrom::Start(0)).unwrap();
    let mut whole = String::new();
    dup.read_to_string(&mut whole).unwrap();
    println!("shared cursor = {}", whole.trim_end().replace('\n', "|"));

    // IntoRawFd releases ownership; FromRawFd re-adopts the same fd.
    let released = dup.into_raw_fd();
    println!("released nonzero = {}", released > 0);
    let mut adopted = unsafe { File::from_raw_fd(released) };
    adopted.seek(SeekFrom::Start(0)).unwrap();
    let mut s2 = String::new();
    adopted.read_to_string(&mut s2).unwrap();
    println!("adopted = {}", s2.trim_end().replace('\n', "|"));
    drop(adopted);

    // OwnedFd round trip: File -> OwnedFd -> File keeps the same fd.
    let owned: OwnedFd = f.into();
    println!("owned fd kept = {}", owned.as_raw_fd() == raw);
    let mut f2 = File::from(owned);
    f2.seek(SeekFrom::Start(0)).unwrap();
    let mut s3 = String::new();
    f2.read_to_string(&mut s3).unwrap();
    println!("owned read = {}", s3.starts_with("fd round trip"));
    drop(f2);

    std::fs::remove_file(&path).unwrap();
    std::fs::remove_dir(&dir).unwrap();
    println!("cleaned = {}", !std::path::Path::new(&dir).exists());
}
