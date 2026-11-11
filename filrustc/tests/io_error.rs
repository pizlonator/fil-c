use std::error::Error as StdError;
use std::fmt;
use std::io::{self, Error, ErrorKind};

fn main() {
    let e = Error::from_raw_os_error(2);
    println!("kind = {:?} raw = {:?}", e.kind(), e.raw_os_error());
    let e2 = Error::new(ErrorKind::InvalidData, "custom message");
    println!("custom kind = {:?}", e2.kind());
    #[derive(Debug)]
    struct MyErr(String);
    impl fmt::Display for MyErr {
        fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
            write!(f, "my: {}", self.0)
        }
    }
    impl StdError for MyErr {}
    let e3: Box<dyn StdError> = Box::new(MyErr("str error".to_string()));
    println!("downcast custom = {:?}", e3.downcast_ref::<MyErr>().map(|e| e.0.as_str()));
    let e4: Box<dyn StdError> = Box::new(Error::other("owned".to_string()));
    println!("boxed io err len > 0 = {}", format!("{}", e4).len() > 0);
    let e5: Box<dyn StdError> = "static str".into();
    println!("from str = {}", e5.to_string());

    #[derive(Debug)]
    struct Wrapper { source: Error, note: String }
    impl fmt::Display for Wrapper {
        fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
            write!(f, "wrapper: {}", self.note)
        }
    }
    impl StdError for Wrapper {
        fn source(&self) -> Option<&(dyn StdError + 'static)> {
            Some(&self.source)
        }
    }
    let w = Wrapper { source: Error::from_raw_os_error(13), note: "inner".to_string() };
    let boxed: Box<dyn StdError> = Box::new(w);
    println!("display = {}", boxed);
    let mut chain = 0;
    let mut cur: Option<&(dyn StdError + 'static)> = Some(boxed.as_ref());
    while let Some(e) = cur {
        chain += 1;
        cur = e.source();
    }
    println!("chain length = {}", chain);
    let boxed2: Box<dyn StdError + Send + Sync> = Box::new(Error::new(ErrorKind::Other, "sync err"));
    println!("sync boxed = {}", boxed2);

    let kinds = [ErrorKind::NotFound, ErrorKind::PermissionDenied, ErrorKind::InvalidInput, ErrorKind::UnexpectedEof];
    println!("kinds = {:?}", kinds);
    let w2 = boxed.downcast::<Wrapper>().unwrap();
    println!("downcast box = {} source kind = {:?}", w2.note, w2.source.kind());
    println!("io err into box = {}", Box::<dyn StdError>::from(io::Error::from_raw_os_error(5)).to_string().len() > 0);
}
