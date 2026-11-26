#!/usr/bin/env python3
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
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL FILIP PIZLO OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# triage.py -- builds TRIAGE.md from the last run-x86sem-tests run.
#
# For every test dir it reads test-output/x86sem/<dir>/ (cmp verdict,
# cmp.out detail, emu.stderr banner, native.stderr) and classifies the
# failure:
#   PASS                     -- compared clean
#   missing-mnemonic         -- emu.stderr names "unimplemented <mnem>"
#   emu-died(<reason>)       -- emulator fatal error mid-guest (trace
#                               truncated)
#   native-died(sig)         -- the hardware guest faulted (trace
#                               truncated; a real hardware fault the
#                               emulator either avoids or hits later)
#   value-divergence         -- equal record counts, a GPR/ymm slot
#                               differs (silent wrong-value bug)
#   flag-divergence          -- equal record counts, only flag bits
#                               differ
#   count-mismatch           -- neither side left a reason; inspect
#                               manually
# and resolves the instruction at the first divergence via objdump.
# Output: TRIAGE.md with a per-dir table, a categorized inventory and a
# summary table by mnemonic family. Re-run after any emulator fix
# batch: it always describes the CURRENT test-output.

import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "test-output", "x86sem")


def dirs():
    out = []
    for d in sorted(os.listdir(OUT)):
        p = os.path.join(OUT, d, "cmp")
        if os.path.isdir(os.path.join(OUT, d)):
            out.append(d)
    return out


def read(path, limit=4):
    try:
        with open(path, errors="replace") as f:
            return "".join(f.readline() for _ in range(limit))
    except OSError:
        return ""


def objdump_insn(binary, rip):
    try:
        out = subprocess.run(["objdump", "-d", binary], capture_output=True,
                             text=True, timeout=60).stdout
    except Exception:
        return None
    pat = re.compile(r"^\s*([0-9a-f]+):\t([0-9a-f ]+)\t(.+)$")
    insns = []
    for line in out.splitlines():
        m = pat.match(line)
        if m:
            insns.append((int(m.group(1), 16), len(m.group(2).split()),
                          m.group(3).strip()))
    want = int(rip, 16)
    for i, (start, n, text) in enumerate(insns):
        if start < want <= start + n:
            return "#%d  %s" % (i + 1, text)
    return None


def classify(d):
    base = os.path.join(OUT, d)
    cmp_stamp = os.path.join(base, "cmp")
    cmp_out = read(os.path.join(base, "cmp.out"), 12)
    cmp_err = read(os.path.join(base, "cmp.err"), 6)
    emu_err = read(os.path.join(base, "emu.stderr"), 3)
    nat_err = read(os.path.join(base, "native.stderr"), 2)

    if os.path.exists(cmp_stamp):
        m = re.search(r"COMPARE OK (\d+) records, (\d+) compared"
                      r"(, (\d+) flag-bit mismatch\(es\) skipped.*)?", cmp_out)
        if m:
            note = ""
            if m.group(3):
                note = " (%s undefined-flag bit mismatches skipped)" % m.group(4)
            return "PASS", "%s records%s" % (m.group(1), note), None
        return "PASS", "stamp, no detail", None

    # Failure path.
    m = re.search(r"unimplemented (\S+)", emu_err)
    if m:
        return "missing-mnemonic", m.group(1), None
    m = re.search(r"fatal guest error \(tid=\d+\): (.*?) at rip=(\S+)",
                  emu_err)
    if m:
        return "emu-died", "%s at %s" % (m.group(1), m.group(2)), None
    m = re.search(r"guest fault sig=(\d+) rip=(\S+) .*after (\d+) steps",
                  nat_err)
    if m:
        return "native-died", "sig=%s after %s steps (rip %s)" % (
            m.group(1), m.group(3), m.group(2)), None

    m = re.search(r"record count mismatch: native=(\d+) emu=(\d+)", cmp_out)
    counts = (m.group(1), m.group(2)) if m else None

    m = re.search(r"first divergence at record (\d+) \(instruction #(\d+), "
                  r"rip (\S+)\)", cmp_out)
    if not m:
        if counts:
            return "count-mismatch", "native=%s emu=%s" % counts, None
        return "no-data", (cmp_out.strip() or cmp_err.strip())[:120], None

    rec, insn_no, rip = m.group(1), m.group(2), m.group(3)
    slot = None
    ms = re.search(r"  slot: (.+)", cmp_out)
    if ms:
        slot = ms.group(1).strip()
    nv = re.search(r"  native: (\S+)", cmp_out)
    ev = re.search(r"  emu:    (\S+)", cmp_out)
    binary = os.path.join(base, "test")
    insn = objdump_insn(binary, rip) or "?"
    detail = "rec %s (insn %s) rip %s slot %s insn %s" % (
        rec, insn_no, rip, slot, insn)
    if counts:
        detail = "native=%s emu=%s; then %s" % (counts[0], counts[1], detail)
    if slot and slot.startswith("fl:"):
        return "flag-divergence", detail, (slot, nv.group(1) if nv else "?",
                                           ev.group(1) if ev else "?")
    return "value-divergence", detail, (slot, nv.group(1) if nv else "?",
                                        ev.group(1) if ev else "?")


FAMILY = {
    "bsf": "bsf/bsr (bit scan)", "bsr": "bsf/bsr (bit scan)",
    "shl": "shifts/rotates", "sal": "shifts/rotates", "shr":
    "shifts/rotates", "sar": "shifts/rotates", "sarx": "shifts/rotates",
    "shld": "shifts/rotates", "shrd": "shifts/rotates",
    "div": "div/idiv", "idiv": "div/idiv",
    "vzeroall": "vzeroall/vzeroupper", "vzeroupper": "vzeroall/vzeroupper",
    "pcmpestri": "SSE4.2 string compare", "pcmpestrm":
    "SSE4.2 string compare", "pcmpistri": "SSE4.2 string compare",
    "pcmpistrm": "SSE4.2 string compare",
    "rcpss": "rcp/rsqrt", "rcpps": "rcp/rsqrt", "rsqrtss": "rcp/rsqrt",
    "rsqrtps": "rcp/rsqrt",
    "adc": "adc/xadd carry", "xadd": "adc/xadd carry",
    "scasb": "string scan/compare", "scas": "string scan/compare",
    "lodsb": "string load", "movsb": "string move",
    "vfmadd": "FMA", "vfmsub": "FMA", "vfnmadd": "FMA", "vfnmsub": "FMA",
    "vfmaddsub": "FMA", "vfmsubadd": "FMA",
    "mpsadbw": "mpsadbw", "vmpsadbw": "mpsadbw",
    "vcvtps2ph": "f16 convert", "vcvtph2ps": "f16 convert",
    "cvtsd2si": "x87/SSE converts",
    "sqrtss": "sqrt", "sqrtps": "sqrt", "sqrtsd": "sqrt", "sqrtpd": "sqrt",
}


def family_of(detail):
    m = re.search(r"insn #\d+  (\S+)", detail) or re.search(r"unimplemented "
                                                            r"(\S+)", detail)
    if not m:
        return "other"
    mn = m.group(1).lower().rstrip(":;,")
    for suffix in ("b", "w", "l", "q"):
        if mn.endswith(suffix) and mn[:-1] in FAMILY:
            return FAMILY[mn[:-1]]
    for key in sorted(FAMILY, key=len, reverse=True):
        if mn.startswith(key):
            return FAMILY[key]
    return "other:" + mn


def main():
    rows = []
    for d in dirs():
        cls, detail, vals = classify(d)
        rows.append((d, cls, detail, vals))
    passes = [r for r in rows if r[1] == "PASS"]
    fails = [r for r in rows if r[1] != "PASS"]

    lines = []
    lines.append("# x86sem differential suite: TRIAGE REPORT")
    lines.append("")
    lines.append("Generated by triage.py from the current "
                 "test-output/x86sem/ state; re-run after each fix batch.")
    lines.append("")
    lines.append("## Summary")
    lines.append("")
    lines.append("| class | count |")
    lines.append("|---|---|")
    by_class = {}
    for d, cls, detail, vals in rows:
        by_class[cls] = by_class.get(cls, 0) + 1
    for cls in sorted(by_class):
        lines.append("| %s | %d |" % (cls, by_class[cls]))
    lines.append("| total | %d |" % len(rows))
    lines.append("")

    fam_count = {}
    for d, cls, detail, vals in fails:
        fam = family_of(detail)
        fam_count[fam] = fam_count.get(fam, 0) + 1
    lines.append("## Failure families (prioritized by count)")
    lines.append("")
    lines.append("| family | failing dirs |")
    lines.append("|---|---|")
    for fam, n in sorted(fam_count.items(), key=lambda kv: -kv[1]):
        lines.append("| %s | %d |" % (fam, n))
    lines.append("")

    lines.append("## Full per-dir inventory (failing first)")
    lines.append("")
    lines.append("| dir | class | first divergence / reason |")
    lines.append("|---|---|---|")
    for d, cls, detail, vals in fails:
        text = detail.replace("|", "\\|")
        lines.append("| %s | %s | %s |" % (d, cls, text))
    for d, cls, detail, vals in passes:
        text = detail.replace("|", "\\|")
        lines.append("| %s | %s | %s |" % (d, cls, text))
    lines.append("")

    lines.append("## Divergence value samples")
    lines.append("")
    lines.append("First-divergence slot values for the value/flag classes "
                 "(native = hardware, emu = emulator):")
    lines.append("")
    lines.append("| dir | slot | native | emu |")
    lines.append("|---|---|---|---|")
    for d, cls, detail, vals in fails:
        if not vals:
            continue
        slot, nv, ev = vals
        nv, ev = nv[:64], ev[:64]
        lines.append("| %s | %s | %s | %s |" % (d, slot, nv, ev))
    lines.append("")

    with open(os.path.join(HERE, "TRIAGE.md"), "w") as f:
        f.write("\n".join(lines) + "\n")
    print("TRIAGE.md written: %d dirs, %d pass, %d fail"
          % (len(rows), len(passes), len(fails)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
