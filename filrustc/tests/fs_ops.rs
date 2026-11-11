use std::fs;
use std::fs::{File, OpenOptions};
use std::io::{prelude::*, Seek, SeekFrom};
use std::path::Path;

fn main() {
    let dir = format!("/tmp/filc-fs-{}", std::process::id());
    fs::create_dir_all(&dir).unwrap();
    let path = format!("{}/file.txt", dir);

    let mut f = OpenOptions::new().read(true).write(true).create(true).truncate(true).open(&path).unwrap();
    f.write_all(b"hello fs").unwrap();
    writeln!(f, " second line").unwrap();
    f.sync_all().unwrap();
    f.seek(SeekFrom::Start(0)).unwrap();
    let mut first = String::new();
    f.read_to_string(&mut first).unwrap();
    println!("read back = {}", first.trim_end().replace('\n', "|"));
    drop(f);

    let contents = fs::read_to_string(&path).unwrap();
    println!("helper = {}", contents.trim_end().len());
    let bytes = fs::read(&path).unwrap();
    println!("bytes = {}", bytes.len());

    fs::write(format!("{}/quick.txt", dir), b"quick").unwrap();
    println!("quick = {}", fs::read_to_string(format!("{}/quick.txt", dir)).unwrap());

    let meta = fs::metadata(&path).unwrap();
    println!("is_file = {} len = {}", meta.is_file(), meta.len());
    let perms = meta.permissions();
    println!("readonly = {}", perms.readonly());
    let mut writable = perms.clone();
    writable.set_readonly(true);
    fs::set_permissions(&path, writable).unwrap();
    println!("now readonly = {}", fs::metadata(&path).unwrap().permissions().readonly());
    let mut writable2 = fs::metadata(&path).unwrap().permissions();
    writable2.set_readonly(false);
    fs::set_permissions(&path, writable2).unwrap();

    let mut append = OpenOptions::new().append(true).open(&path).unwrap();
    writeln!(append, "appended").unwrap();
    drop(append);
    let final_len = fs::metadata(&path).unwrap().len();
    println!("after append len = {}", final_len);

    let mut rw = OpenOptions::new().read(true).write(true).open(&path).unwrap();
    let mut buf = [0u8; 5];
    rw.read_exact(&mut buf).unwrap();
    println!("openoptions read = {}", String::from_utf8(buf.to_vec()).unwrap());

    let sub = format!("{}/sub", dir);
    fs::create_dir(&sub).unwrap();
    println!("is_dir = {}", Path::new(&sub).is_dir());
    fs::write(format!("{}/sub/f2.txt", dir), b"subfile").unwrap();

    let entries: Vec<String> = {
        let mut names = Vec::new();
        for e in fs::read_dir(&dir).unwrap() {
            let e = e.unwrap();
            names.push(e.file_name().to_string_lossy().to_string());
        }
        names.sort();
        names
    };
    println!("entries = {}", entries.join(","));

    let canonical = fs::canonicalize(&path).unwrap();
    println!("canonical ends = {}", canonical.to_string_lossy().ends_with("file.txt"));

    let renamed = format!("{}/renamed.txt", dir);
    fs::rename(&path, &renamed).unwrap();
    println!("renamed exists = {} old exists = {}", Path::new(&renamed).exists(), Path::new(&path).exists());

    // symlink + read_link
    #[cfg(unix)]
    {
        let link = format!("{}/link.txt", dir);
        std::os::unix::fs::symlink(&renamed, &link).unwrap();
        let target = fs::read_link(&link).unwrap();
        println!("read_link = {}", target.to_string_lossy().ends_with("renamed.txt"));
        println!("via link = {}", fs::read_to_string(&link).unwrap().len() == final_len as usize);
        fs::remove_file(&link).unwrap();
    }

    fs::remove_file(&renamed).unwrap();
    fs::remove_file(format!("{}/quick.txt", dir)).unwrap();
    fs::remove_file(format!("{}/sub/f2.txt", dir)).unwrap();
    fs::remove_dir(&sub).unwrap();
    fs::remove_dir_all(&dir).unwrap();
    println!("cleaned = {}", !Path::new(&dir).exists());
}
