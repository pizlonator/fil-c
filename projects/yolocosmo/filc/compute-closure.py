#!/usr/bin/env python3
#-*-mode:python;coding:utf-8;tab-width:4;indent-tabs-mode:nil-*-

# Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Computes the set of cosmopolitan libc objects that the Fil-C yolo layer
(libpizlo.a + the yolo crt chain + the pizlonated user libc + libc++) can
reference, and emits filc/filc-objects.mk, which the cosmo Makefile uses to
build the slim pizfix/lib/libyolocosmo.a archive (see build_yolocosmo.sh).

The full cosmopolitan.a contains everything cosmo builds (zlib, ncurses,
mbedtls, the entire test/tool trees, ...).  Fil-C only ever resolves symbols
from it on behalf of:

  - libpizlo.a             (the yolo side of the runtime: pthreads, futexes,
                            mmap, signals, stdio, and the zsys_* syscall
                            pass-throughs),
  - the yolo crt chain     (crt1.o = cosmo's crt.o + yologlue.o, ape.o,
                            crtbegin.o/crtend.o, filc_crt.o, filc_mincrt.o),
  - pizfix/lib/libc.a      (the pizlonated user libc: its members reference
                            the plain yolo implementations of some libc
                            functions),
  - pizfix/lib/libc++*.a   (the pizlonated C++ runtimes),
  - any static library compiled by the host toolchain that gets linked into
    a Fil-C cosmo binary (today: the vendored luau inside minilute, whose
    references to plain math/stdlib entry points resolve from -lyolocosmo),

so everything else in cosmopolitan.a is dead weight.  This script:

  1. indexes the symbols of every member of the full cosmopolitan.a,
  2. seeds the link simulation with the undefined symbols of all of the
     consumers above (for archives that means every member, since any member
     can be pulled by some program),
  3. repeatedly pulls the cosmo members that define still-undefined symbols,
     exactly like ld does when scanning an archive,
  4. treats libyolort.a (compiler-rt builtins + crt) and libyolounwind.a as
     providers, since the driver links them inside the same --start-group as
     -lyolocosmo, and re-scans them after every cosmo pull,
  5. writes the resulting object list as a make fragment, and reports what it
     did (counts, sizes, per-tree breakdown, unresolved/conflicting symbols).

The script must be run from a tree that has a complete cosmo build (i.e. after
`make m=x86_64-ape toolchain` in projects/yolocosmo and a full cosmo
flavor build of pizfix).  It never has to be run by hand: the checked-in
filc-objects.mk is the source of truth for the build; this tool exists to
regenerate it when cosmo or the runtime changes what gets referenced.

Usage (from anywhere):

    python3 projects/yolocosmo/filc/compute-closure.py [--mode x86_64-ape]

The mode name selects the architecture too (like the cosmo Makefile does):
a mode containing "aarch64" computes filc-objects-aarch64.mk from the
pizfix/lib-aarch64 consumers; everything else computes filc-objects.mk from
the pizfix/lib consumers.
"""

import argparse
import collections
import glob
import multiprocessing
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))


def die(msg):
    sys.stderr.write("compute-closure: " + msg + "\n")
    sys.exit(1)


# nm symbol types that can satisfy an undefined reference from another object.
# 'U' is undefined, 'w' is a weak *undefined* (never forces an archive pull),
# lowercase (other than 'u') are local symbols, and '?' means nm could not
# make sense of the symbol.  Everything else - T t? no: T D B R W V C u N A G
# i s and their lowercase-but-global variants - counts as a definition.
NON_DEFINING = set("Uw?")


class Obj(object):
    """A linkable object (a file or an archive member)."""

    __slots__ = ("name", "path", "defined", "undefined", "size")

    def __init__(self, name, path, defined, undefined, size=0):
        self.name = name
        self.path = path
        self.defined = defined
        self.undefined = undefined
        self.size = size


HEX_RE = re.compile(r"^[0-9a-fA-F]+$")


def parse_body(body):
    """Parses the part of an `nm` output line that follows the file/member
    prefix: "[address] type symbol" (the address is absent for undefined and
    some other symbol types).  The symbol name may contain spaces (the
    embedded-zip zoneinfo objects define symbols named after file paths).
    Returns (type, symbol) or None."""
    tokens = body.split(None, 1)
    if len(tokens) == 1:
        return None
    first = tokens[0]
    if HEX_RE.match(first) and first.isdigit() or (HEX_RE.match(first) and
                                                   len(first) > 1 and
                                                   first.lstrip("0")):
        # an address, then "type symbol"
        rest = tokens[1]
        toks = rest.split(None, 1)
        if len(toks) != 2:
            return None
        return toks[0], toks[1]
    return first, tokens[1].strip()


def classify(syms):
    defined = set()
    undefined = set()
    for kind, sym in syms:
        if kind in NON_DEFINING:
            undefined.add(sym)
        elif kind.isupper() or kind in ("u", "v"):
            defined.add(sym)
        # anything else (lowercase local) is ignored
    return defined, undefined


def nm_archive(path):
    """Returns a list of Obj, one per member occurrence (cosmo archives keep
    duplicate member names - e.g. libc/sock/bind.o and third_party/readline/
    bind.o both show up as bind.o - and each occurrence is a distinct archive
    member that ld pulls independently, so they must not be merged)."""
    out = subprocess.run(["nm", "-A", path], capture_output=True, text=True)
    if out.returncode != 0:
        die("nm failed on %s: %s" % (path, out.stderr))
    prefix = path + ":"
    occurrences = []          # [(name, [(kind, sym), ...]), ...]
    current_name = None
    current = None
    for line in out.stdout.splitlines():
        if not line.startswith(prefix):
            continue
        rest = line[len(prefix):]
        member = rest.split(":", 1)[0]
        if member != current_name:
            # a new block of lines for a (possibly repeating) member name
            current_name = member
            current = []
            occurrences.append((member, current))
        parsed = parse_body(rest[len(member) + 1:])
        if parsed is not None:
            current.append(parsed)
    result = []
    for member, syms in occurrences:
        if not syms:
            continue
        defined, undefined = classify(syms)
        result.append(Obj(member, path, defined, undefined))
    return result


def nm_object(path):
    out = subprocess.run(["nm", "-A", path], capture_output=True, text=True)
    if out.returncode != 0:
        die("nm failed on %s: %s" % (path, out.stderr))
    prefix = path + ":"
    syms = []
    for line in out.stdout.splitlines():
        if not line.startswith(prefix):
            continue
        parsed = parse_body(line[len(prefix):])
        if parsed is not None:
            syms.append(parsed)
    defined, undefined = classify(syms)
    return Obj(os.path.basename(path), path, defined, undefined,
               os.path.getsize(path))


def nm_object_task(path):
    try:
        return path, nm_object(path)
    except SystemExit:
        return path, None


def nm_objects_parallel(paths):
    result = {}
    with multiprocessing.Pool(min(32, os.cpu_count() or 1)) as pool:
        for path, obj in pool.imap_unordered(nm_object_task, paths, chunksize=16):
            result[path] = obj
    return result


def main():
    parser = argparse.ArgumentParser(
        description="Regenerate filc/filc-objects.mk (see the docstring).")
    parser.add_argument("--mode", default="x86_64-ape",
                        help="cosmo build mode whose o/ tree to scan")
    parser.add_argument("--output", default=None,
                        help="where to write the make fragment "
                             "(default: projects/yolocosmo/filc/"
                             "filc-objects.mk, or "
                             "projects/yolocosmo/filc/filc-objects-aarch64.mk "
                             "for an aarch64 mode)")
    args = parser.parse_args()

    # The architecture follows the mode name, exactly like the cosmo
    # Makefile's ARCH derivation (any mode containing "aarch64" is aarch64).
    arch = "aarch64" if "aarch64" in args.mode else "x86_64"

    # The pizfix tree keeps one lib directory per target architecture ("lib"
    # for x86_64 and "lib-aarch64" for aarch64); the headers are shared.
    libdir = "lib-aarch64" if arch == "aarch64" else "lib"

    odir = os.path.join(REPO, "projects", "yolocosmo", "o", args.mode)
    pizfix = os.path.join(REPO, "pizfix")

    cosmopolitan_a = os.path.join(odir, "cosmopolitan.a")
    if not os.path.exists(cosmopolitan_a):
        die("missing %s; build the full cosmo tree first "
            "(make m=%s toolchain)" % (cosmopolitan_a, args.mode))

    # ── 1. index the full archive ────────────────────────────────────────────
    print("indexing %s ..." % os.path.relpath(cosmopolitan_a, REPO))
    members = nm_archive(cosmopolitan_a)
    print("  %d members" % len(members))

    # Map each archive member to the o/.../*.o file that make would build.
    # Most member names are unique basenames within the o tree; the handful of
    # colliding basenames (e.g. libc/sock/bind.o vs third_party/readline/
    # bind.o) are disambiguated by comparing each member's symbol signature
    # against the candidate files.
    print("scanning o/ tree for candidate objects ...")
    by_basename = collections.defaultdict(list)
    for dirpath, _dirnames, filenames in os.walk(odir):
        for fn in filenames:
            if fn.endswith(".o"):
                by_basename[fn].append(os.path.join(dirpath, fn))

    def resolve(member, sig):
        candidates = by_basename.get(member, [])
        if len(candidates) == 1:
            return [candidates[0]]
        if len(candidates) > 1:
            nmed = nm_objects_parallel(candidates)
            matches = []
            for cand in sorted(candidates):
                obj = nmed[cand]
                if obj is None:
                    continue
                if obj.defined == sig[0] and obj.undefined == sig[1]:
                    matches.append(cand)
            if matches:
                return matches
            # The archive member may be stale relative to the o/ tree (e.g.
            # the archive was packed before a flag change rebuilt some
            # objects, so the on-disk object's undefined-symbol set has
            # drifted).  Fall back to matching on the defined-symbol set
            # alone: what ld pulls a member for is what it defines.
            for cand in sorted(candidates):
                obj = nmed[cand]
                if obj is not None and obj.defined == sig[0]:
                    matches.append(cand)
            if matches:
                return matches
        return []

    member_to_paths = [None] * len(members)
    unresolved_members = []
    for idx, member in enumerate(members):
        paths = resolve(member.name, (member.defined, member.undefined))
        if not paths:
            unresolved_members.append(member.name)
            continue
        member_to_paths[idx] = paths
    if unresolved_members:
        print("  note: %d members have no matching object file (stale "
              "archive?): %s" % (len(unresolved_members),
                                 " ".join(sorted(unresolved_members)[:10])))

    # ── 2. the consumers whose undefined symbols seed the closure ───────────
    # The C++ runtime archives are required on x86_64 (the flavor is fully
    # built there); on aarch64 they may not exist yet (libc++ for aarch64 is
    # optional), in which case the closure is computed without them and has
    # to be regenerated once they exist.
    seed_archives = [
        os.path.join(pizfix, libdir, "libpizlo.a"),
        os.path.join(pizfix, libdir, "libc.a"),
    ]
    optional_cxx_archives = [
        os.path.join(pizfix, libdir, "libc++.a"),
        os.path.join(pizfix, libdir, "libc++abi.a"),
        os.path.join(pizfix, libdir, "libc++experimental.a"),
    ]
    required_cxx = arch == "x86_64"

    # Fil-C binaries can also link static libraries that were compiled by the
    # host toolchain (i.e. un-pizlonated C/C++ code whose references to plain
    # libc symbols resolve from -lyolocosmo).  Today that is exactly one
    # thing: the vendored luau inside minilute (see build_minilute.sh), which
    # build_base.sh builds in the cosmo flavor too - and only for the host
    # architecture (x86_64); there is no aarch64 minilute build.  Its
    # references to math/string/stdlib entry points are why entries like
    # libc/tinymath/log10.o are in the x86_64 list even though nothing
    # pizlonated calls them.
    if arch == "x86_64":
        seed_archives += sorted(
            glob.glob(os.path.join(REPO, "projects", "lute-1.0.0", "extern",
                                   "luau", "build", "release", "*.a")))
    seed_objects = [
        os.path.join(pizfix, libdir, "crt1.o"),
        os.path.join(pizfix, libdir, "cosmo-crt.o"),
        os.path.join(pizfix, libdir, "crtbegin.o"),
        os.path.join(pizfix, libdir, "crtend.o"),
        os.path.join(pizfix, libdir, "filc_crt.o"),
        os.path.join(pizfix, libdir, "filc_mincrt.o"),
        os.path.join(pizfix, libdir, "yologlue.o"),
        os.path.join(pizfix, libdir, "libm.a"),
    ]
    # x86_64 additionally links pizfix/lib/ape.o (the APE header blobs from
    # ape.S); aarch64 links no such object (see the Makefile's filcyolo).
    if arch == "x86_64":
        seed_objects.append(os.path.join(pizfix, "lib", "ape.o"))
    # libyolort.a and libyolounwind.a are providers: they sit in the same
    # --start-group as -lyolocosmo and are re-scanned after every cosmo pull,
    # so a cosmo member may reference their symbols freely.
    provider_archives = [
        os.path.join(pizfix, libdir, "libyolort.a"),
        os.path.join(pizfix, libdir, "libyolounwind.a"),
    ]

    undefined = set()
    defined_elsewhere = set()

    print("seeding from the yolo-side consumers ...")
    missing_seed = [p for p in seed_archives if not os.path.exists(p)]
    optional = [p for p in missing_seed if "/luau/build/" in p]
    if optional:
        print("  note: skipping %d optional seed archives (not built): %s" %
              (len(optional), " ".join(os.path.relpath(p, REPO)
                                       for p in optional)))
    for path in seed_archives + provider_archives:
        if not os.path.exists(path):
            if "/luau/build/" in path:
                continue
            die("missing %s; build the cosmo flavor of pizfix first" % path)
        for obj in nm_archive(path):
            undefined |= obj.undefined
            defined_elsewhere |= obj.defined
    cxx_seeded = []
    for path in optional_cxx_archives:
        if not os.path.exists(path):
            if required_cxx:
                die("missing %s; build the cosmo flavor of pizfix first"
                    % path)
            continue
        cxx_seeded.append(path)
        for obj in nm_archive(path):
            undefined |= obj.undefined
            defined_elsewhere |= obj.defined
    if cxx_seeded and len(cxx_seeded) < len(optional_cxx_archives):
        print("  note: only %d of the %d C++ runtime seed archives were "
              "found; regenerate the closure once they all exist"
              % (len(cxx_seeded), len(optional_cxx_archives)))
    for path in seed_objects:
        if not os.path.exists(path):
            continue
        obj = nm_object(path)
        undefined |= obj.undefined
        defined_elsewhere |= obj.defined

    # The x86_64 ape.lds linker script also references symbols of its own,
    # in its header arithmetic: CHURN(WinMain) hashes the Windows PE entry
    # point into the APE UUID, so every x86_64 cosmo-mode link requires
    # WinMain to be defined even though no object references it.  (EfiMain is
    # only read under DEFINED(EfiMain), so it stays optional.)  Seed it so
    # that the pull loop pulls libc/runtime/winmain.greg.o from the archive
    # exactly like ld does when it scans -lyolocosmo after the linker script.
    # The aarch64 aarch64.lds has no such arithmetic (apelink generates the
    # headers), so it needs no WinMain seed.
    if arch == "x86_64":
        undefined |= {"WinMain"}

    # The seed archives' own definitions satisfy their mutual references, but
    # (crucially) they do NOT provide symbols for cosmo members: libc.a is
    # scanned before -lyolocosmo in the final link, so a reference that first
    # appears from inside a cosmo member can only be satisfied by another
    # cosmo member or by libyolort.a/libyolounwind.a.  We therefore keep the
    # seed undefined set intact and only remove the seeds' definitions of
    # symbols they themselves define (which are also re-seeded as undefined by
    # other members anyway).
    undefined -= defined_elsewhere

    # Members that must never be pulled, per architecture (each with a
    # reason).  On aarch64 cosmo's own outline-atomic routines collide with
    # the ones compiler-rt's builtins provide (libyolort.a), which the
    # Fil-C-generated code calls directly; compiler-rt wins, cosmo's copy is
    # dead weight, and leaving both in makes every link fail with duplicate
    # __aarch64_cas*/swp* definitions.
    excludes = {
        "aarch64": {"libc/intrin/aarch64/atomics.o"},
        "x86_64": set(),
    }[arch]

    # ── 3+4. pull cosmo members to fixpoint ─────────────────────────────────
    # `undefined` holds every symbol that the seeded consumers reference and
    # that nothing pulled so far defines.  Pulling a member adds its
    # references and removes its definitions, mirroring ld's archive scan.
    # A member pulled later may re-reference a symbol an earlier pull
    # already defined, which is why definitions are tracked separately and
    # the subtraction happens on every update.
    print("computing the transitive closure ...")
    pulled_idx = set()
    cosmo_defined = set()
    conflicts = []
    progress = True
    while progress:
        progress = False
        for idx, member in enumerate(members):
            if idx in pulled_idx:
                continue
            paths = member_to_paths[idx]
            if paths and any(os.path.relpath(x, odir) in excludes
                             for x in paths):
                continue
            if member.defined & undefined:
                pulled_idx.add(idx)
                clash = member.defined & cosmo_defined
                if clash:
                    conflicts.append((member.name, clash))
                cosmo_defined |= member.defined
                undefined |= member.undefined
                undefined -= cosmo_defined
                progress = True

    # Collect the object paths, in cosmopolitan.a member order: ld scans an
    # archive front to back, so keeping the same relative order as the full
    # archive keeps the slim link behaving exactly like the full one.
    selected = []
    seen_paths = set()
    missing_paths = []
    for idx, member in enumerate(members):
        if idx not in pulled_idx:
            continue
        paths = member_to_paths[idx]
        if not paths:
            missing_paths.append(member.name)
            continue
        for path in paths:
            if path not in seen_paths:
                seen_paths.add(path)
                selected.append(path)

    # ── report ───────────────────────────────────────────────────────────────
    rel = lambda p: os.path.relpath(p, odir)
    total_bytes = 0
    trees = collections.Counter()
    tree_bytes = collections.Counter()
    for path in selected:
        r = rel(path)
        size = os.path.getsize(path)
        total_bytes += size
        tree = r.split("/")[0]
        trees[tree] += 1
        tree_bytes[tree] += size
    print("")
    print("selected %d objects, %d bytes (%.1f MB)" %
          (len(selected), total_bytes, total_bytes / 1e6))
    for tree, count in sorted(trees.items(), key=lambda kv: -tree_bytes[kv[0]]):
        print("  %-24s %5d objects  %8.2f MB" %
              (tree, count, tree_bytes[tree] / 1e6))
    if missing_paths:
        print("WARNING: %d pulled members have no object file: %s" %
              (len(missing_paths), " ".join(sorted(missing_paths))))
    for name, clash in conflicts:
        print("note: member %s redefines %s (cosmo has duplicate providers; "
              "ld will pick one)" % (name, " ".join(sorted(clash)[:4])))
    still_undefined = undefined - defined_elsewhere
    if still_undefined:
        # References that nothing in the closure or the pizfix libraries
        # defines.  Most are linker-script symbols, references internal to
        # libc.a (a pulled pizlonated member can reference another pizlonated
        # member that this simulation has not pulled - ld resolves those
        # within -lc), and a couple of compiler-rt objects that nothing
        # references today.  ld will fail the link if any of these ever
        # matters; the test suite is the arbiter.
        internal = sorted(s for s in still_undefined
                          if s.startswith("pizlonated"))
        external = sorted(s for s in still_undefined
                          if not s.startswith("pizlonated"))
        print("note: %d symbols referenced by pulled cosmo members remain "
              "undefined outside the closure (%d are libc.a-internal "
              "pizlonated_* references; %d others):" %
              (len(still_undefined), len(internal), len(external)))
        for sym in external:
            print("   %s" % sym)

    # ── emit the make fragment ───────────────────────────────────────────────
    objs = selected
    lines = []
    lines.append("#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;"
                 "coding:utf-8-*-┐")
    lines.append("#── vi: set et ft=make ts=8 sw=8 fenc=utf-8 :vi "
                 "─────────────────────┘")
    lines.append("#")
    lines.append("# Generated by filc/compute-closure.py - DO NOT EDIT.")
    lines.append("#")
    lines.append("# The objects of the full cosmopolitan.a that the Fil-C yolo")
    lines.append("# layer can reference (see compute-closure.py and")
    lines.append("# build_yolocosmo.sh).  Everything else in cosmo (zlib,")
    lines.append("# ncurses, mbedtls, the tool and test trees, ...) is never")
    lines.append("# resolved from the yolo side, so it is not built.")
    lines.append("")
    lines.append("FILC_YOLO_OBJS = \\")
    for i, path in enumerate(objs):
        lines.append("\to/$(MODE)/%s%s" % (rel(path),
                                           " \\" if i < len(objs) - 1 else ""))
    out_path = args.output or os.path.join(
        REPO, "projects", "yolocosmo", "filc",
        "filc-objects-aarch64.mk" if arch == "aarch64" else "filc-objects.mk")
    with open(out_path, "w") as f:
        f.write("\n".join(lines) + "\n")
    print("")
    print("wrote %s (%d objects)" % (os.path.relpath(out_path, REPO), len(objs)))


if __name__ == "__main__":
    main()
