use std::ffi::{CStr, CString, OsStr, OsString};
use std::os::unix::ffi::{OsStrExt, OsStringExt};
use std::path::{Path, PathBuf};

fn main() {
    // CString/CStr round trip
    let c = CString::new("fil-c ffi").unwrap();
    let raw = c.into_raw();
    let back = unsafe { CString::from_raw(raw) };
    println!("cstring roundtrip = {}", back.to_str().unwrap());
    let c2 = CString::new("with bytes").unwrap();
    let cref: &CStr = &c2;
    println!("cstr bytes = {:?} len = {}", cref.to_bytes().len(), cref.to_bytes_with_nul().len());
    println!("to_str = {} lossy = {:?}", cref.to_str().unwrap(), cref.to_str().unwrap().to_string());
    println!("cstr from_ptr = {}", unsafe { CStr::from_ptr(cref.as_ptr()) }.to_str().unwrap());
    println!("nul error = {}", CString::new("a\0b").is_err());
    println!("empty cstr = {:?}", CString::new("").unwrap().to_bytes());

    // OsString/OsStr
    let os = OsString::from("os string");
    println!("os = {} len = {}", os.to_string_lossy(), os.len());
    let os2 = OsString::from_vec(vec![0x66, 0x69, 0x6C]);
    println!("os from_vec = {}", os2.to_string_lossy());
    let s: &OsStr = "borrowed os".as_ref();
    println!("os borrowed = {}", s.to_string_lossy());
    println!("os bytes = {:?}", s.as_bytes().len());
    let os3: OsString = "converted".into();
    println!("os into = {} is-empty = {}", os3.to_string_lossy(), os3.is_empty());
    println!("os cmp = {}", OsString::from("a") < OsString::from("b"));
    println!("os concat = {}", OsString::from("x").to_string_lossy() != "" && true);

    // Path/PathBuf (no fs needed)
    let p = Path::new("/tmp/filc-ffi/sub/file.txt");
    println!("parent = {}", p.parent().unwrap().to_string_lossy());
    println!("file_name = {}", p.file_name().unwrap().to_string_lossy());
    println!("extension = {}", p.extension().unwrap().to_string_lossy());
    println!("components = {}", p.components().count());
    println!("ancestors = {}", p.ancestors().count());
    println!("starts_with = {} strip = {}", p.starts_with("/tmp"), p.strip_prefix("/tmp/filc-ffi").unwrap().to_string_lossy());

    let mut pb = PathBuf::from("/tmp");
    pb.push("filc-ffi");
    pb.push("other.bin");
    pb.set_extension("dat");
    println!("pathbuf = {} ext = {}", pb.to_string_lossy(), pb.extension().unwrap().to_string_lossy());
    println!("join = {}", Path::new("/a").join("b/c").to_string_lossy());
    println!("is_absolute = {} file_stem = {}", pb.is_absolute(), Path::new("x.tar.gz").file_stem().unwrap().to_string_lossy());
    println!("as_os_str = {}", pb.as_os_str().len() > 0);
    let relative = Path::new("rel/path");
    println!("relative abs = {}", relative.is_absolute());
    println!("iter = {}", relative.iter().count());
}
