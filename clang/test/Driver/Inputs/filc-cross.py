import os
import re
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

    def invoke(self, *args, arch=None, triple=None, query=False, success=True):
        command = [
            *runner, str(self.clang), "-no-canonical-prefixes",
            "--no-default-config", "--target=" + (triple or (arch or self.cross) + "-linux-gnu"),
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
        # Preserve filesystem traversal through a symlinked build/bin directory.
        prefix = self.clang.parent / "../.."
        return result.stdout.replace(str(prefix), str(prefix.resolve()))

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

    def test_symlinked_bin_directory_uses_physical_runtime_paths(self):
        physical = self.root / "physical"
        physical.mkdir()
        self.clang.parent.parent.rename(physical / "build")
        self.host_pizfix.rename(physical / "pizfix")
        self.cross_pizfix.rename(physical / ("pizfix-" + self.cross))
        self.clang.parent.parent.mkdir()
        self.clang.parent.symlink_to(physical / "build/bin", target_is_directory=True)
        for arch, name in ((native, "pizfix"), (self.cross, "pizfix-" + self.cross)):
            with self.subTest(arch=arch):
                self.check_runtime(self.invoke(arch=arch), physical / name, arch)

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

    def test_linux_vendor_and_libc_triples_use_packaged_cxx_headers(self):
        for arch, root in ((native, self.host_pizfix), (self.cross, self.cross_pizfix)):
            for suffix in ("-redhat-linux-gnu", "-suse-linux-gnu", "-suse-linux", "-linux-musl"):
                with self.subTest(arch=arch, suffix=suffix):
                    output = self.invoke("-x", "c++", triple=arch + suffix)
                    self.check_runtime(output, root, arch)
                    self.assertIn("/include/" + arch + "-unknown-linux-gnu/c++/v1", output)

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

    def test_unsupported_cross_architecture_has_no_runtime_install_hint(self):
        for arch, args in (("i686", ()), ("x86_64", ("-m32",)), ("arm", ())):
            with self.subTest(arch=arch, args=args):
                output = self.invoke(*args, arch=arch, success=False)
                self.assertIn("unsupported Fil-C target architecture", output)
                self.assertNotIn("install it as", output)
                self.assertNotIn('"-cc1"', output)
        self.invoke("-dumpmachine", arch="i686", query=True)

    def test_cross_without_host_pizfix_requires_explicit_runtime(self):
        # /opt/fil and installed /usr compilers do not have a neighboring pizfix.
        self.host_pizfix.rename(self.root / "unused-host-pizfix")
        for args in (("-c",), ("-c", "--filc-crt-path=/custom-crt")):
            with self.subTest(args=args):
                output = self.invoke(*args, success=False)
                self.assertIn("cross compiling for " + self.cross, output)
                self.assertIn("--filc-resource-dir=", output)
                self.assertNotIn('"-cc1"', output)
        for query in ("-dumpmachine", "--version", "-print-resource-dir"):
            self.invoke(query, query=True)
        output = self.invoke("--filc-resource-dir=" + str(self.cross_pizfix))
        self.check_runtime(output, self.cross_pizfix, self.cross)

    def test_aliases_use_canonical_runtime_headers_and_binutils(self):
        alias = "arm64" if self.cross == "aarch64" else "amd64"
        output = self.invoke("-x", "c++", arch=alias)
        self.check_runtime(output, self.cross_pizfix, self.cross)
        self.assertIn("/include/" + self.cross + "-unknown-linux-gnu/c++/v1", output)
        self.assertIn(str(self.path / (self.cross + "-linux-gnu-ld")), output)
        output = self.invoke("-###", "-c", "-x", "assembler", "/dev/null", arch=alias, query=True)
        self.assertIn('"--as" "' + str(self.path / (self.cross + "-linux-gnu-as")), output)

    def test_cross_assembler_rejects_implicit_host_tools(self):
        (self.path / (self.cross + "-linux-gnu-as")).unlink()
        self.executable(self.path / "as")
        self.executable(self.clang.parent / "as")
        asm = ("--gcc-toolchain=/nonexistent", "-###", "-c", "-x", "assembler", "/dev/null")
        for extra in ((), ("-yolo-assembler", "-fno-integrated-as")):
            with self.subTest(extra=extra):
                output = self.invoke(*asm, *extra, query=True, success=False)
                self.assertIn("cannot find a target assembler", output)
        # An explicit directory or GCC-style filename prefix remains trusted.
        for prefix in (self.root / "target-tools", self.root / "target-prefix-"):
            if prefix.name.endswith("-"):
                tool = self.executable(Path(str(prefix) + "as"))
            else:
                tool = self.executable(prefix / "as")
            output = self.invoke(*asm, "-B" + str(prefix), query=True)
            self.assertIn('"--as" "' + str(tool), output)
        output = self.invoke(*asm, "-yolo-assembler", query=True)
        self.assertIn('"-cc1as"', output)

    def test_cross_gcc_unprefixed_assembler(self):
        (self.path / (self.cross + "-linux-gnu-as")).unlink()
        self.executable(self.path / "as")
        self.executable(self.clang.parent / "as")
        gcc = self.root / "gcc"
        triple = self.cross + "-linux-gnu"
        crt = gcc / "lib/gcc" / triple / "12/crtbegin.o"
        crt.parent.mkdir(parents=True)
        crt.touch()
        tool = self.executable(gcc / triple / "bin/as")
        output = self.invoke("--gcc-toolchain=" + str(gcc), "-###", "-c",
                             "-x", "assembler", "/dev/null", query=True)
        selected = re.search(r'"--as" "([^" ]+)"', output)
        self.assertIsNotNone(selected, output)
        self.assertEqual(Path(selected.group(1)).resolve(), tool)

    def test_vendor_triples_find_gnu_binutils_without_cross_gcc(self):
        for tool in ("as", "ld"):
            self.executable(self.path / tool)
        for suffix in ("-unknown-linux-gnu", "-redhat-linux-gnu", "-suse-linux", "-linux-musl"):
            triple = self.cross + suffix
            with self.subTest(triple=triple):
                output = self.invoke("--gcc-toolchain=/nonexistent", triple=triple)
                self.assertIn(str(self.path / (self.cross + "-linux-gnu-ld")), output)
                output = self.invoke("--gcc-toolchain=/nonexistent", "-###", "-c",
                                     "-x", "assembler", "/dev/null", triple=triple, query=True)
                self.assertIn('"--as" "' + str(self.path / (self.cross + "-linux-gnu-as")), output)
        # Prefer explicitly named vendor tools when both variants are present.
        triple = self.cross + "-suse-linux"
        linker = self.executable(self.path / (triple + "-ld"))
        self.assertIn(str(linker), self.invoke("--gcc-toolchain=/nonexistent", triple=triple))

    def test_cross_assembly_uses_host_sarcasm_and_target_as(self):
        output = self.invoke("-###", "-c", "-x", "assembler", "/dev/null", query=True)
        self.assertIn(str(self.host_sarcasm), output)
        self.assertNotIn(str(self.cross_pizfix / "bin/sarcasm"), output)
        self.assertIn('"--as" "' + str(self.path / (self.cross + "-linux-gnu-as")), output)
        self.assertIn('"--arm64"' if self.cross == "aarch64" else '"--x86_64"', output)

    def test_cross_preprocessed_assembly(self):
        output = self.invoke(
            "-###", "-c", "-x", "assembler-with-cpp", "/dev/null", query=True,
        )
        self.assertIn('"-E"', output)
        self.assertIn(str(self.host_sarcasm), output)
        self.assertNotIn(str(self.cross_pizfix / "bin/sarcasm"), output)
        self.assertIn('"--as" "' + str(self.path / (self.cross + "-linux-gnu-as")), output)

    def test_yolo_assembler_override_in_both_directions(self):
        for arch in (native, self.cross):
            with self.subTest(arch=arch):
                output = self.invoke(
                    "-###", "-c", "-yolo-assembler", "-x", "assembler", "/dev/null",
                    arch=arch, query=True,
                )
                self.assertIn('"-cc1as"', output)
                self.assertIn(arch + "-unknown-linux-gnu", output)
                self.assertNotIn(str(self.host_sarcasm), output)
                self.assertNotIn(str(self.cross_pizfix / "bin/sarcasm"), output)

    def test_cross_shared_library(self):
        output = self.invoke("-shared")
        self.assertIn('"-shared"', output)
        self.assertIn(str(self.cross_pizfix / "lib"), output)
        self.assertNotIn(str(self.host_pizfix / "lib"), output)
        self.assertNotIn('"-dynamic-linker"', output)

    def test_kernel_header_override_keeps_target_runtime(self):
        headers = self.root / "custom-kernel-headers"
        output = self.invoke("--filc-os-include=" + str(headers))
        self.assertIn(str(headers), output)
        self.assertNotIn(str(self.cross_pizfix / "os-include"), output)
        self.assertIn(str(self.cross_pizfix / "include"), output)
        self.assertIn(str(self.cross_pizfix / ("lib/ld-fil1-" + self.cross + ".so")), output)

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
