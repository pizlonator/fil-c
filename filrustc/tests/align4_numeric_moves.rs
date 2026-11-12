// Mirror of filc/tests/bug294 (C): align-4, non-multiple-of-8 float
// aggregates (Vec3/Quat/Transform shapes) copied BY VALUE at 4-mod-8 offsets
// through #[inline(never)] call chains.  These aggregates are numeric, so
// they must keep the untyped byte-array allocas and upstream SROA's integer
// widening -- the Fil-C union handling (zhas_union/zmemmove_builtin) does not
// apply to them -- and every by-value copy must preserve the values exactly,
// including both the whole-aggregate assignment (r.q = t.q) and the
// field-by-field variant the C test's FILC_REPRO_SCALAR_COPY path takes.
#[repr(C)]
#[derive(Clone, Copy)]
struct Vec3 {
    x: f32,
    y: f32,
    z: f32,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct Quat {
    v: Vec3,
    s: f32,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct Transform {
    p: Vec3,
    q: Quat,
}

const ZERO: Vec3 = Vec3 { x: 0.0, y: 0.0, z: 0.0 };

#[inline(never)]
fn cross(a: Vec3, b: Vec3) -> Vec3 {
    Vec3 {
        x: a.y * b.z - a.z * b.y,
        y: a.z * b.x - a.x * b.z,
        z: a.x * b.y - a.y * b.x,
    }
}

#[inline(never)]
fn inv_rotate_vector(q: Quat, v: Vec3) -> Vec3 {
    let t1 = cross(q.v, v);
    let t2 = Vec3 { x: t1.x + q.s * v.x, y: t1.y + q.s * v.y, z: t1.z + q.s * v.z };
    let t3 = cross(q.v, t2);
    Vec3 { x: v.x - 2.0 * t3.x, y: v.y - 2.0 * t3.y, z: v.z - 2.0 * t3.z }
}

#[inline(never)]
fn to_relative_transform(t: Transform, base: Vec3) -> Transform {
    // the whole-aggregate assignment (the C test's default path)
    Transform { q: t.q, p: Vec3 { x: t.p.x - base.x, y: t.p.y - base.y, z: t.p.z - base.z } }
}

#[inline(never)]
fn to_relative_transform_fieldwise(t: Transform, base: Vec3) -> Transform {
    // the C test's FILC_REPRO_SCALAR_COPY path: field-by-field copies that
    // land on 4-mod-8 offsets inside the 28-byte aggregate
    let mut r = Transform { p: ZERO, q: Quat { v: ZERO, s: 0.0 } };
    r.q.v.x = t.q.v.x;
    r.q.v.y = t.q.v.y;
    r.q.v.z = t.q.v.z;
    r.q.s = t.q.s;
    r.p.x = t.p.x - base.x;
    r.p.y = t.p.y - base.y;
    r.p.z = t.p.z - base.z;
    r
}

#[inline(never)]
fn invert_transform(t: Transform) -> Transform {
    Transform {
        p: inv_rotate_vector(t.q, Vec3 { x: -t.p.x, y: -t.p.y, z: -t.p.z }),
        q: Quat { v: Vec3 { x: -t.q.v.x, y: -t.q.v.y, z: -t.q.v.z }, s: t.q.s },
    }
}

#[inline(never)]
fn refresh_cache(xf: Transform) -> (f32, Transform) {
    let mesh = to_relative_transform(xf, ZERO);
    let result = invert_transform(mesh);
    (result.p.x + result.q.v.z, result)
}

fn main() {
    println!(
        "sizes {} {} {}",
        std::mem::size_of::<Vec3>(),
        std::mem::size_of::<Quat>(),
        std::mem::size_of::<Transform>()
    );
    println!(
        "aligns {} {} {}",
        std::mem::align_of::<Vec3>(),
        std::mem::align_of::<Quat>(),
        std::mem::align_of::<Transform>()
    );
    let xf = Transform {
        p: Vec3 { x: 1.0, y: 2.0, z: 3.0 },
        q: Quat { v: Vec3 { x: 0.1, y: 0.2, z: 0.3 }, s: 0.9 },
    };
    // run the by-value chain twice (once per to_relative_transform variant)
    // and through a Vec for good measure
    let (sink, result) = refresh_cache(xf);
    let mut v: Vec<Transform> = Vec::new();
    v.push(to_relative_transform_fieldwise(xf, ZERO));
    let sink2 = { let result2 = invert_transform(v.pop().unwrap()); result2.p.x + result2.q.v.z };
    println!("sink = {:.5} {:.5}", sink, sink2);
    let ok = (result.p.x + 1.0).abs() < 1e-4
        && (result.p.y + 2.0).abs() < 1e-4
        && (result.p.z + 3.0).abs() < 1e-4
        && (result.q.v.x + 0.1).abs() < 1e-4
        && (result.q.v.y + 0.2).abs() < 1e-4
        && (result.q.v.z + 0.3).abs() < 1e-4
        && (result.q.s - 0.9).abs() < 1e-4
        && (sink - sink2).abs() < 1e-6;
    println!("{}", if ok { "ok" } else { "bad" });
    assert!(ok);
}
