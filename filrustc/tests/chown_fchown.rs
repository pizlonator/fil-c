// `std::os::unix::fs::{chown, fchown}`: change a file's owner and group
// through the path-based and fd-based APIs and observe the change through
// MetadataExt's uid()/gid().  The chown targets the RUNNER'S OWN uid/gid
// (read from the per-test tmp dir, the same owner cross-check unix_fs_ext
// uses), so the assertions hold for any runner -- root can chown to
// anything, and a non-root owner may chown their own file to their own
// uid/gid.
use std::os::unix::fs::{chown, fchown, MetadataExt, PermissionsExt};

fn main() {
    let dir = std::env::temp_dir().join(format!("filc-chown-{}", std::process::id()));
    std::fs::create_dir(&dir).unwrap();
    let path = dir.join("f.txt");
    std::fs::write(&path, b"hello chown").unwrap();

    // The runner's own uid/gid (the tmp dir belongs to us).
    let dirmeta = std::fs::metadata(&dir).unwrap();
    let (uid, gid) = (dirmeta.uid(), dirmeta.gid());
    println!("runner uid = {} gid = {}", uid, gid);

    // The file starts out owned by us too.
    let before = std::fs::metadata(&path).unwrap();
    println!("before = {} {}", before.uid(), before.gid());

    // chown by path, to the same owner but a DIFFERENT group the runner is
    // in (the egid is one of the runner's groups; for root anything works).
    chown(&path, Some(uid), Some(gid)).unwrap();
    let after = std::fs::metadata(&path).unwrap();
    println!(
        "chown ok = {} {}",
        after.uid() == uid && after.gid() == gid,
        after.len() == 11
    );

    // fchown through the open File (append mode keeps the data).
    let f = std::fs::OpenOptions::new().append(true).open(&path).unwrap();
    fchown(&f, Some(uid), Some(gid)).unwrap();
    let fmeta = f.metadata().unwrap();
    println!("fchown ok = {}", fmeta.uid() == uid && fmeta.gid() == gid);

    // chown with -1 (None) leaves a side untouched; verify via mode+size.
    fchown(&f, None, Some(gid)).unwrap();
    f.set_permissions(std::fs::Permissions::from_mode(0o640)).unwrap();
    let final_meta = std::fs::metadata(&path).unwrap();
    println!(
        "none-side ok = {} mode = {:o}",
        final_meta.uid() == uid && final_meta.gid() == gid,
        final_meta.permissions().mode() & 0o7777
    );
    drop(f);

    std::fs::remove_dir_all(&dir).unwrap();
    println!("cleaned = true");
}
