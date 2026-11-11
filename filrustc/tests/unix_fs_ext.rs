// std::os::unix::fs extensions: FileExt (read_at/write_at at multiple offsets,
// cursor independence), MetadataExt (uid/gid/size/dev/ino/atime/mtime/nlink),
// PermissionsExt (mode bits + set_permissions/from_mode round trip), and
// OpenOptionsExt (custom_flags + mode + append through the ext trait).
use std::fs::{self, OpenOptions};
use std::io::{prelude::*, Seek, SeekFrom};
use std::os::unix::fs::{FileExt, MetadataExt, OpenOptionsExt, PermissionsExt};

fn main() {
    let dir = format!("/tmp/filc-unixfs-{}", std::process::id());
    fs::create_dir_all(&dir).unwrap();
    let path = format!("{}/ext.dat", dir);

    // FileExt: paged I/O at explicit offsets.  The handle is read+write
    // (File::create alone is O_WRONLY, and paged READS through it are
    // legitimately EBADF).
    let mut f = OpenOptions::new()
        .read(true)
        .write(true)
        .create(true)
        .truncate(true)
        .open(&path)
        .unwrap();
    f.write_all(b"0123456789abcdefghij").unwrap();
    f.write_at(b"AAAA", 4).unwrap();
    f.write_at(b"BBBB", 12).unwrap();
    let mut buf = [0u8; 4];
    let n = f.read_at(&mut buf, 4).unwrap();
    println!("at4 = {} n = {}", String::from_utf8_lossy(&buf), n);
    f.read_exact_at(&mut buf, 12).unwrap();
    println!("at12 = {}", String::from_utf8_lossy(&buf));
    f.write_all_at(b"CC", 18).unwrap();

    // read_at/write_at must leave the sequential cursor where it was.
    f.seek(SeekFrom::Start(0)).unwrap();
    let mut seq = [0u8; 2];
    f.read_exact(&mut seq).unwrap(); // "01"
    f.read_exact_at(&mut buf, 4).unwrap(); // "AAAA", cursor untouched
    let mut seq2 = [0u8; 2];
    f.read_exact(&mut seq2).unwrap(); // "23" (cursor was at 2)
    println!("cursor kept = {}{}", String::from_utf8_lossy(&seq), String::from_utf8_lossy(&seq2));

    // MetadataExt on a real file (and the directory for an owner cross-check,
    // so the assertions hold for any uid/gid the suite runs under).
    let meta = fs::metadata(&path).unwrap();
    let dirmeta = fs::metadata(&dir).unwrap();
    println!("size = {}", meta.len() == 20);
    println!("same owner = {}", meta.uid() == dirmeta.uid() && meta.gid() == dirmeta.gid());
    println!("dev nonzero = {} ino nonzero = {}", meta.dev() > 0, meta.ino() > 0);
    println!("nlink = {}", meta.nlink());
    println!("atime positive = {} mtime positive = {}", meta.atime() > 0, meta.mtime() > 0);

    // PermissionsExt: explicit mode round trips (chmod, so umask is out of
    // play) through set_mode/set_permissions and from_mode.
    let mut perms = meta.permissions();
    perms.set_mode(0o600);
    fs::set_permissions(&path, perms).unwrap();
    println!("mode now = {:o}", fs::metadata(&path).unwrap().permissions().mode() & 0o777);
    fs::set_permissions(&path, fs::Permissions::from_mode(0o640)).unwrap();
    println!("from_mode = {:o}", fs::metadata(&path).unwrap().permissions().mode() & 0o777);
    println!("readonly = {}", fs::metadata(&path).unwrap().permissions().readonly());

    // OpenOptionsExt: custom_flags composes with the std flags (O_CLOEXEC =
    // 0o2000000 on Linux, benign), plus the ext trait's mode() and append.
    let mut app = OpenOptions::new()
        .custom_flags(0o2000000)
        .mode(0o600)
        .append(true)
        .open(&path)
        .unwrap();
    writeln!(app, "appended").unwrap();
    drop(app);
    println!("append len = {}", fs::metadata(&path).unwrap().len() == 29);

    fs::remove_file(&path).unwrap();
    fs::remove_dir_all(&dir).unwrap();
    println!("cleaned = {}", !std::path::Path::new(&dir).exists());
}
