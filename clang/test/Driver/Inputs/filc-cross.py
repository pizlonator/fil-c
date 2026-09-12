import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


# Optional arguments after the compiler are a runner, e.g.
# /usr/bin/qemu-aarch64 -L /usr/aarch64-linux-gnu.
compiler = Path(sys.argv[1]).absolute()
runner = sys.argv[2:]
native = subprocess.check_output(
    [*runner, str(compiler), "--no-default-config", "-dumpmachine"], text=True
).strip().split("-")[0]
native = {"arm64": "aarch64", "amd64": "x86_64"}.get(native, native)


@unittest.skipUnless(native in ("x86_64", "aarch64"), "requires a Fil-C host")
class FilCCrossTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.clang = self.root / "build/bin/clang"
        self.clang.parent.mkdir(parents=True)
        self.clang.symlink_to(compiler)
        self.cross = "aarch64" if native == "x86_64" else "x86_64"
        self.host_pizfix = self.root / "pizfix"
        self.cross_pizfix = self.root / ("pizfix-" + self.cross)
        self.host_pizfix.mkdir()
        self.cross_pizfix.mkdir()
        self.path = self.root / "host-tools"
        self.path.mkdir()
        self.env = dict(os.environ, PATH=str(self.path))
        self.env.pop("COMPILER_PATH", None)
        self.env.pop("CCC_OVERRIDE_OPTIONS", None)
        for arch in (native, self.cross):
            for tool in ("as", "ld"):
                self.executable(self.path / (arch + "-linux-gnu-" + tool))
        self.host_sarcasm = self.host_pizfix / "bin/sarcasm"
        self.executable(self.host_sarcasm)
        self.executable(self.cross_pizfix / "bin/sarcasm")

    def executable(self, path):
        path.parent.mkdir(parents=True, exist_ok=True)
        # These tools must never actually run, even if this test regresses.
        path.write_text("#!/bin/sh\nexit 99\n")
        path.chmod(0o755)
        return path

    def invoke(self, *args, arch=None, query=False, success=True):
        command = [
            *runner, str(self.clang), "-no-canonical-prefixes",
            "--no-default-config", "--target=" + (arch or self.cross) + "-linux-gnu",
        ]
        if not query:
            command += ["-###", "-x", "c"]
        command += list(args)
        if not query:
            command += ["/dev/null"]
        result = subprocess.run(
            command, env=self.env, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        )
        self.assertEqual(result.returncode == 0, success, result.stdout)
        # Native paths retain ../.., whereas selected cross paths are normalized.
        return result.stdout.replace(str(self.clang.parent / "../.."), str(self.root))

    def check_runtime(self, output, root, arch):
        self.assertIn(str(root / "include"), output)
        self.assertIn(str(root / "os-include"), output)
        self.assertIn(str(root / ("lib/ld-fil1-" + arch + ".so")), output)

    def test_native_and_cross_runtime(self):
        for arch, root in ((native, self.host_pizfix), (self.cross, self.cross_pizfix)):
            with self.subTest(arch=arch):
                output = self.invoke(arch=arch)
                self.check_runtime(output, root, arch)
                self.assertIn(str(root / "lib/Scrt1.o"), output)

    def test_crt_override_keeps_target_headers_and_loader(self):
        crt = self.root / "custom-crt"
        output = self.invoke("--filc-crt-path=" + str(crt))
        self.check_runtime(output, self.cross_pizfix, self.cross)
        self.assertIn(str(crt / "Scrt1.o"), output)
        self.assertNotIn(str(self.host_pizfix / "include"), output)

    def test_resource_override(self):
        override = self.root / "custom-pizfix"
        self.cross_pizfix.rename(override)
        for extra in ((), ("--filc-crt-path=/custom-crt",)):
            with self.subTest(extra=extra):
                output = self.invoke("--filc-resource-dir=" + str(override), *extra)
                self.check_runtime(output, override, self.cross)

    def test_cxx_headers_in_both_directions(self):
        for arch in (native, self.cross):
            with self.subTest(arch=arch):
                output = self.invoke("--driver-mode=g++", "-x", "c++", arch=arch)
                self.assertIn("/include/" + arch + "-unknown-linux-gnu/c++/v1", output)
                self.assertIn("/include/c++/v1", output)
                self.assertIn('"-lc++"', output)

    def test_missing_runtime_and_queries(self):
        self.cross_pizfix.rename(self.root / "unused-pizfix")
        for args in ((), ("--filc-crt-path=/custom-crt",)):
            with self.subTest(args=args):
                output = self.invoke(*args, success=False)
                self.assertIn("cross compiling for " + self.cross, output)
                self.assertIn(str(self.cross_pizfix), output)
                self.assertNotIn('"-cc1"', output)
        for query in ("-dumpmachine", "--version", "-print-resource-dir"):
            with self.subTest(query=query):
                self.invoke(query, query=True)

    def test_aliases_use_canonical_runtime_headers_and_binutils(self):
        alias = "arm64" if self.cross == "aarch64" else "amd64"
        output = self.invoke("-x", "c++", arch=alias)
        self.check_runtime(output, self.cross_pizfix, self.cross)
        self.assertIn("/include/" + self.cross + "-unknown-linux-gnu/c++/v1", output)
        self.assertIn(str(self.path / (self.cross + "-linux-gnu-ld")), output)
        output = self.invoke("-###", "-c", "-x", "assembler", "/dev/null", arch=alias, query=True)
        self.assertIn('"--as" "' + str(self.path / (self.cross + "-linux-gnu-as")), output)

    def test_cross_assembly_uses_host_sarcasm_and_target_as(self):
        output = self.invoke("-###", "-c", "-x", "assembler", "/dev/null", query=True)
        self.assertIn(str(self.host_sarcasm), output)
        self.assertNotIn(str(self.cross_pizfix / "bin/sarcasm"), output)
        self.assertIn('"--as" "' + str(self.path / (self.cross + "-linux-gnu-as")), output)
        self.assertIn('"--arm64"' if self.cross == "aarch64" else '"--x86_64"', output)

    def test_cross_sarcasm_fallback_avoids_target_tool_directories(self):
        self.host_sarcasm.unlink()
        host_sarcasm = self.executable(self.path / "sarcasm")
        target_tools = self.root / "target-tools"
        target_sarcasm = self.executable(target_tools / "sarcasm")
        target_as = self.executable(target_tools / "as")
        output = self.invoke(
            "-###", "-c", "-x", "assembler", "/dev/null", "-B" + str(target_tools),
            query=True,
        )
        self.assertIn(str(host_sarcasm), output)
        self.assertNotIn(str(target_sarcasm), output)
        self.assertIn('"--as" "' + str(target_as), output)

        # /opt/fil-style installs keep host sarcasm beside the compiler.
        adjacent_sarcasm = self.executable(self.clang.parent / "sarcasm")
        output = self.invoke(
            "-###", "-c", "-x", "assembler", "/dev/null", "-B" + str(target_tools),
            query=True,
        )
        self.assertIn(str(adjacent_sarcasm), output)
        self.assertNotIn(str(target_sarcasm), output)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
