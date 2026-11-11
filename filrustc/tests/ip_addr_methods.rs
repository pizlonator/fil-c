// IpAddr/Ipv4Addr/Ipv6Addr method families: predicates (is_loopback/
// is_multicast/is_broadcast/is_private/is_link_local/is_unspecified/
// is_unique_local), octets/segments/from_octets/from_segments/to_bits/
// from_bits, v4<->v6 conversions (to_ipv6_mapped/to_ipv6_compatible/
// to_ipv4/to_ipv4_mapped/to_canonical), constants (LOCALHOST/BROADCAST/
// UNSPECIFIED), FromStr parsing, Display, and the IpAddr enum wrappers.
// Pure computation -- no networking.
use std::net::{IpAddr, Ipv4Addr, Ipv6Addr};

fn main() {
    println!("v4 localhost = {}", Ipv4Addr::LOCALHOST);
    println!("v4 broadcast = {}", Ipv4Addr::BROADCAST);
    println!("v4 unspecified = {}", Ipv4Addr::UNSPECIFIED);
    let a = Ipv4Addr::new(192, 168, 1, 9);
    println!("octets = {:?}", a.octets());
    println!("from_bits = {}", Ipv4Addr::from_bits(u32::from_be_bytes(a.octets())) == a);
    println!("bits = {} back = {}", a.to_bits(), Ipv4Addr::from_bits(a.to_bits()) == a);
    println!(
        "private = {} link_local = {} multicast = {} broadcast = {}",
        Ipv4Addr::new(10, 0, 0, 1).is_private(),
        Ipv4Addr::new(169, 254, 1, 1).is_link_local(),
        Ipv4Addr::new(224, 0, 0, 1).is_multicast(),
        Ipv4Addr::BROADCAST.is_broadcast()
    );
    println!(
        "loopback = {} unspecified = {}",
        Ipv4Addr::LOCALHOST.is_loopback(),
        Ipv4Addr::UNSPECIFIED.is_unspecified()
    );

    println!("v6 localhost = {}", Ipv6Addr::LOCALHOST);
    println!("v6 unspecified = {}", Ipv6Addr::UNSPECIFIED.is_unspecified());
    let v6 = Ipv6Addr::new(0x2001, 0xdb8, 0, 0, 0, 0, 0, 1);
    println!("v6 display = {}", v6);
    println!("segments = {:?}", v6.segments());
    let seg = v6.segments();
    println!("segments back = {}", Ipv6Addr::new(seg[0], seg[1], seg[2], seg[3], seg[4], seg[5], seg[6], seg[7]) == v6);
    println!("octets back = {}", Ipv6Addr::from_bits(u128::from_be_bytes(v6.octets())) == v6);
    println!(
        "v6 loopback = {} unique_local = {} multicast = {}",
        Ipv6Addr::LOCALHOST.is_loopback(),
        Ipv6Addr::new(0xfc00, 0, 0, 0, 0, 0, 0, 1).is_unique_local(),
        Ipv6Addr::new(0xff02, 0, 0, 0, 0, 0, 0, 1).is_multicast()
    );

    // v4 <-> v6 conversions.
    let mapped = Ipv4Addr::new(192, 0, 2, 9).to_ipv6_mapped();
    println!("mapped = {}", mapped);
    println!(
        "to_ipv4_mapped = {} compatible = {}",
        mapped.to_ipv4_mapped() == Some(Ipv4Addr::new(192, 0, 2, 9)),
        Ipv4Addr::new(192, 0, 2, 9).to_ipv6_compatible().to_ipv4()
            == Some(Ipv4Addr::new(192, 0, 2, 9))
    );
    let v6u = Ipv6Addr::new(0, 0, 0, 0, 0, 0xffff, 0xc000, 0x0209);
    println!("to_ipv4 = {} canonical = {}", v6u.to_ipv4().unwrap(), v6u.to_canonical());

    // The IpAddr enum: From conversions, predicates, equality with the
    // concrete types, canonicalization.
    let ip = IpAddr::from(a);
    println!("ip = {} is_ipv4 = {} is_ipv6 = {}", ip, ip.is_ipv4(), ip.is_ipv6());
    println!(
        "ip predicates = {} {} {}",
        ip.is_loopback(),
        Ipv4Addr::new(239, 1, 2, 3).is_multicast(),
        IpAddr::from(Ipv6Addr::LOCALHOST).is_loopback()
    );
    println!("eq concrete = {}", ip == a && ip != Ipv6Addr::LOCALHOST);
    println!("canonical = {}", IpAddr::from(v6u).to_canonical());

    // FromStr + Display.
    let p4: Ipv4Addr = "127.0.0.1".parse().unwrap();
    let p6: Ipv6Addr = "::1".parse().unwrap();
    println!(
        "parsed = {} {}",
        p4 == Ipv4Addr::LOCALHOST,
        p6 == Ipv6Addr::LOCALHOST
    );
    println!("bad v4 err = {}", "not an ip".parse::<Ipv4Addr>().is_err());
    println!("bad v6 err = {}", "::fffff".parse::<Ipv6Addr>().is_err());
    let pip: IpAddr = "192.168.1.9".parse().unwrap();
    println!("ip parsed = {} matches = {}", pip, pip == ip);
    println!("display v4 = {}", format!("{}", a));
    println!("display v6 = {}", format!("{}", Ipv6Addr::LOCALHOST));
}
