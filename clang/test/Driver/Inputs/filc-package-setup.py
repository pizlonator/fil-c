import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest


# Generate the complete installer using package-build.sh and a tiny fake build.
# Only packaging tools are stubbed; setup runs real mkdir/ln/test commands.
# Redirect /usr to a fixture so tests cannot depend on the machine's headers.
source = Path(sys.argv[1]).resolve()


class PackageSetupTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.usr = self.root / "usr"
        self.tools = self.root / "tools"
        self.tools.mkdir()
        self.env = dict(os.environ, PATH=str(self.tools) + os.pathsep + os.environ["PATH"])
        self.env.pop("FILC_KERNEL_HEADERS", None)
        self.env["PATCHELF_LOG"] = str(self.root / "patchelf.log")
        self.tool("strip", "exit 0")
        self.tool("tar", "exit 0")
        self.env.pop("PATCHELF_FAIL_OPTION", None)
        patchelf = self.tools / "patchelf"
        # Read ELF type/program headers, so scripts, objects, and DSOs exercise
        # distinct probe outcomes. Mutations are logged rather than performed.
        patchelf.write_text("#!" + sys.executable + "\n" + '''import os
from pathlib import Path
import struct
import sys
option = sys.argv[1]
if option == os.environ.get("PATCHELF_FAIL_OPTION"):
    sys.stderr.write("injected patchelf failure: " + option + "\\n")
    sys.exit(9)
path = Path(sys.argv[-1])
data = path.read_bytes()
if data[:4] != b"\\x7fELF" or struct.unpack_from("<H", data, 16)[0] not in (2, 3):
    sys.exit(1)
if option == "--print-interpreter":
    phoff = struct.unpack_from("<Q", data, 32)[0]
    phsize, phnum = struct.unpack_from("<HH", data, 54)
    sys.exit(not any(struct.unpack_from("<I", data, phoff + i * phsize)[0] == 3
                     for i in range(phnum)))
if option == "--print-rpath":
    sys.exit(0)
if option in ("--set-rpath", "--set-interpreter"):
    with open(os.environ["PATCHELF_LOG"], "a") as log:
        log.write(option + " " + str(path) + "\\n")
else:
    sys.exit(1)
''')
        patchelf.chmod(0o755)

    def elf(self, interpreter=False, elf_type=3):
        # Minimal ELF64 headers suffice for the mocked tools; no code executes.
        ident = b"\x7fELF" + bytes((2, 1, 1)) + bytes(9)
        header = ident + struct.pack("<HHIQQQIHHHHHH", elf_type, 62, 1, 0,
                                     64, 0, 0, 64, 56, int(interpreter), 0, 0, 0)
        if interpreter:
            name = b"/lib/ld.so\0"
            header += struct.pack("<IIQQQQQQ", 3, 4, 120, 0, 0, len(name), len(name), 1)
            header += name
        return header

    def tool(self, name, body):
        path = self.tools / name
        path.write_text("#!/bin/sh\n" + body + "\n")
        path.chmod(0o755)

    def generate(self, target, host, success=True):
        self.tool("uname", "echo " + host)
        self.target = target
        other = "aarch64" if target == "x86_64" else "x86_64"
        files = {
            "libpas/common.sh": f"OS=linux\nARCH={target}\nCROSSARCH={other}\n",
            "build_os_include.sh": (source.parent / "build_os_include.sh").read_text(),
            "README.md": "fixture\n",
            "LLVM-LICENSE.txt": "fixture\n",
            "libpas/LICENSE.txt": "fixture\n",
            "projects/usermusl/COPYRIGHT": "fixture\n",
            "build/bin/clang-20": "fixture\n",
            "build/include/c++/v1/vector": "fixture\n",
            f"build/include/{target}-unknown-linux-gnu/c++/v1/__config_site": "fixture\n",
            "build/lib/clang/20/include/stddef.h": "fixture\n",
            "pizfix/lib/libyoloc.so": self.elf(),
            "pizfix/lib/libfixture.so": self.elf(),
            "pizfix/bin/program": self.elf(interpreter=True),
            "pizfix/bin/script": "#!/bin/sh\nexit 0\n",
            "pizfix/bin/object": self.elf(elf_type=1),
            "pizfix/bin/archive": "!<arch>\n",

        }
        for name, content in files.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content if isinstance(content, bytes) else content.encode())
        (self.root / f"pizfix/lib/ld-fil1-{target}.so").symlink_to("libyoloc.so")
        result = subprocess.run(
            ["sh", str(source)], cwd=self.root, env=self.env, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        if not success:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            return result
        self.assertEqual(result.returncode, 0, result.stderr)
        self.package = next(self.root.glob(f"filc-*-linux-{target}"))
        setup = self.package / "setup.sh"
        setup.write_text(setup.read_text().replace("/usr/", str(self.usr) + "/"))
        subprocess.run(["sh", "-n", str(setup)], check=True)
        (self.root / "patchelf.log").unlink()

    def layout(self, target, kind):
        generic = self.usr / "include"
        if kind == "cross-package":
            generic = self.usr / (target + "-linux-gnu/include")
        asm = generic / "asm"
        if kind == "multiarch":
            asm = generic / (target + "-linux-gnu/asm")
        for path in (asm, generic / "linux", generic / "asm-generic"):
            path.mkdir(parents=True, exist_ok=True)
            (path / "types.h").write_text("/* header fixture */\n")
        self.arch_headers(asm, target)
        return asm, generic

    def arch_headers(self, asm, arch):
        if arch in ("x86_64", "amd64"):
            (asm / "posix_types_64.h").write_text("/* x86-64 UAPI */\n")
        else:
            (asm / "hwcap.h").write_text("#define HWCAP_ASIMD (1 << 1)\n")

    def run_setup(self, success=True):
        result = subprocess.run(
            ["sh", "setup.sh"], cwd=self.package, env=self.env, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        if success:
            self.assertIn("You are all set", result.stdout)
            self.assertTrue((self.root / "patchelf.log").exists())
            self.assertNotIn("warning:", result.stderr)
        else:
            self.assertIn("error: missing " + self.target + " kernel headers", result.stderr)
            self.assertIn("FILC_KERNEL_HEADERS", result.stderr)
            self.assertNotIn("You are all set", result.stdout)
            self.assertFalse((self.root / "patchelf.log").exists())
        return result

    def check_links(self, asm, generic):
        headers = self.package / "pizfix/os-include"
        for name, path in (("asm", asm), ("linux", generic / "linux"),
                           ("asm-generic", generic / "asm-generic")):
            self.assertEqual((headers / name).readlink(), path)
            self.assertTrue((headers / name).is_dir())

    def test_source_build_checks_headers_before_building_tools(self):
        self.tool("uname", "echo x86_64")
        self.package = self.root
        script = self.root / "build_os_include.sh"
        script.write_text((source.parent / "build_os_include.sh").read_text().replace(
            "/usr/", str(self.usr) + "/"))
        script.chmod(0o755)
        first_build = self.root / "build_projeny_yolo.sh"
        first_build.write_text('#!/bin/sh\necho started > "$BUILD_LOG"\nexit 77\n')
        first_build.chmod(0o755)
        log = self.root / "build.log"
        self.env["BUILD_LOG"] = str(log)
        command = ["sh", str(source.parent / "build_base.sh")]
        result = subprocess.run(command, cwd=self.root, env=self.env,
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("error: missing x86_64 kernel headers", result.stderr)
        self.assertFalse(log.exists(), "started building before checking headers")
        asm, generic = self.layout("x86_64", "flat")
        result = subprocess.run(command, cwd=self.root, env=self.env,
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 77, result.stderr)
        self.assertTrue(log.exists())
        self.check_links(asm, generic)

    def test_relocation_distinguishes_executables_libraries_and_scripts(self):
        self.generate("x86_64", "x86_64")
        self.layout("x86_64", "flat")
        setup = (self.package / "setup.sh").read_text()
        for name in ("script", "object", "archive"):
            self.assertNotIn("pizfix/bin/" + name, setup)
        self.run_setup()
        operations = (self.root / "patchelf.log").read_text().splitlines()
        self.assertEqual(sorted(operations), sorted([
            "--set-rpath pizfix/lib/libfixture.so",
            "--set-rpath pizfix/bin/program",
            "--set-interpreter pizfix/bin/program",
        ]))
        self.assertEqual((self.package / "pizfix/bin/script").read_text(),
                         "#!/bin/sh\nexit 0\n")

    def test_packaged_cxx_config_for_both_architectures(self):
        self.generate("aarch64", "aarch64")
        for arch in ("x86_64", "aarch64"):
            config = self.package / f"build/include/{arch}-unknown-linux-gnu/c++/v1/__config_site"
            self.assertEqual(config.read_text(), "fixture\n")

    def test_missing_headers_fail_before_relocation_and_can_be_retried(self):
        self.generate("x86_64", "x86_64")
        self.run_setup(success=False)
        self.assertFalse((self.package / "pizfix/os-include").exists())
        asm, generic = self.layout("x86_64", "flat")
        self.run_setup()
        self.check_links(asm, generic)
        self.run_setup()
        self.check_links(asm, generic)

    def test_explicit_headers_with_spaces(self):
        self.generate("aarch64", "x86_64")
        generic = self.root / "target sysroot/include"
        for name in ("asm", "linux", "asm-generic"):
            (generic / name).mkdir(parents=True)
            (generic / name / "types.h").write_text("/* header fixture */\n")
        self.arch_headers(generic / "asm", "aarch64")
        # Relative paths must also become valid absolute symlink targets.
        self.env["FILC_KERNEL_HEADERS"] = "../target sysroot/include"
        self.run_setup()
        self.check_links(generic / "asm", generic)

    def test_incomplete_explicit_headers_do_not_fall_back_to_host(self):
        self.generate("x86_64", "x86_64")
        self.layout("x86_64", "flat")
        generic = self.root / "incomplete"
        generic.mkdir()
        self.env["FILC_KERNEL_HEADERS"] = str(generic)
        self.run_setup(success=False)

    def test_nonexistent_explicit_headers(self):
        self.generate("aarch64", "x86_64")
        self.env["FILC_KERNEL_HEADERS"] = str(self.root / "nonexistent")
        self.run_setup(success=False)

    def test_explicit_relative_headers_ignore_cdpath(self):
        self.generate("aarch64", "x86_64")
        generic = self.package / "headers"
        for name in ("asm", "linux", "asm-generic"):
            (generic / name).mkdir(parents=True)
            (generic / name / "types.h").write_text("/* header fixture */\n")
        self.arch_headers(generic / "asm", "aarch64")
        self.env["FILC_KERNEL_HEADERS"] = "headers"
        self.env["CDPATH"] = str(self.package)
        self.run_setup()
        self.check_links(generic / "asm", generic)

    def test_wrong_architecture_override_preserves_existing_links(self):
        self.generate("x86_64", "x86_64")
        asm, generic = self.layout("x86_64", "flat")
        self.run_setup()
        (self.root / "patchelf.log").unlink()
        _, wrong = self.layout("aarch64", "cross-package")
        self.env["FILC_KERNEL_HEADERS"] = str(wrong)
        self.run_setup(success=False)
        self.check_links(asm, generic)

    def test_wrong_architecture_named_cross_package(self):
        self.generate("aarch64", "x86_64")
        asm, _ = self.layout("aarch64", "cross-package")
        (asm / "hwcap.h").unlink()
        self.arch_headers(asm, "x86_64")
        self.run_setup(success=False)

    def test_mixed_architecture_override(self):
        self.generate("aarch64", "aarch64")
        asm, generic = self.layout("aarch64", "flat")
        self.arch_headers(asm, "x86_64")
        self.env["FILC_KERNEL_HEADERS"] = str(generic)
        self.run_setup(success=False)

    def test_arm32_override_is_not_arm64(self):
        self.generate("aarch64", "aarch64")
        asm, generic = self.layout("aarch64", "flat")
        (asm / "hwcap.h").write_text("#define HWCAP_NEON (1 << 12)\n")
        self.env["FILC_KERNEL_HEADERS"] = str(generic)
        self.run_setup(success=False)

    def test_link_failure_stops_setup_before_relocation(self):
        self.generate("x86_64", "x86_64")
        self.layout("x86_64", "flat")
        self.tool("ln", "echo 'injected link failure' >&2\nexit 9")
        result = subprocess.run(
            ["sh", "setup.sh"], cwd=self.package, env=self.env, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("injected link failure", result.stderr)
        self.assertNotIn("You are all set", result.stdout)
        self.assertFalse((self.root / "patchelf.log").exists())

    def test_relocation_failure_does_not_report_success(self):
        self.generate("x86_64", "x86_64")
        self.layout("x86_64", "flat")
        self.tool("patchelf", "echo 'injected relocation failure' >&2\nexit 9")
        result = subprocess.run(
            ["sh", "setup.sh"], cwd=self.package, env=self.env, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("injected relocation failure", result.stderr)
        self.assertNotIn("You are all set", result.stdout)

    def test_packaging_requires_patchelf_before_removing_existing_output(self):
        self.generate("x86_64", "x86_64")
        sentinel = self.package / "previous-package"
        sentinel.write_text("keep me")
        (self.tools / "patchelf").unlink()
        for name in ("sh", "rm", "mkdir", "cp", "ln", "cat", "chmod"):
            (self.tools / name).symlink_to(shutil.which(name))
        self.env["PATH"] = str(self.tools)
        result = subprocess.run(["sh", str(source)], cwd=self.root,
                                env=self.env, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("patchelf is required", result.stderr)
        self.assertEqual(sentinel.read_text(), "keep me")

    def test_packaging_rpath_failure_is_fatal(self):
        self.env["PATCHELF_FAIL_OPTION"] = "--set-rpath"
        result = self.generate("x86_64", "x86_64", success=False)
        self.assertIn("injected patchelf failure: --set-rpath", result.stderr)

    def test_packaging_interpreter_failure_is_fatal(self):
        self.env["PATCHELF_FAIL_OPTION"] = "--set-interpreter"
        result = self.generate("x86_64", "x86_64", success=False)
        self.assertIn("injected patchelf failure: --set-interpreter", result.stderr)

    def test_failed_rerun_preserves_existing_header_links(self):
        self.generate("x86_64", "x86_64")
        asm, generic = self.layout("x86_64", "flat")
        self.run_setup()
        (self.root / "patchelf.log").unlink()
        self.env["FILC_KERNEL_HEADERS"] = str(self.root / "nonexistent")
        self.run_setup(success=False)
        self.check_links(asm, generic)

    def test_existing_directory_is_not_silently_used_as_a_link_destination(self):
        self.generate("x86_64", "x86_64")
        self.layout("x86_64", "flat")
        existing = self.package / "pizfix/os-include/asm"
        existing.mkdir(parents=True)
        result = subprocess.run(
            ["sh", "setup.sh"], cwd=self.package, env=self.env, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("You are all set", result.stdout)
        self.assertFalse((self.root / "patchelf.log").exists())
        self.assertFalse((existing / "asm").exists())


def header_case(target, host, layout, missing=None, broken_file=False):
    def test(self):
        self.generate(target, host)
        asm, generic = self.layout(target, layout)
        native = {"amd64": "x86_64", "arm64": "aarch64"}.get(host, host) == target
        # Install a complete host tree even in cross tests; it must not mask
        # missing target headers or supply the generic half of a cross sysroot.
        if not native and layout != "flat":
            self.layout(host, "flat")
        if missing:
            path = asm if missing == "asm" else generic / missing
            if broken_file:
                path = path / "types.h"
                path.unlink()
            else:
                shutil.rmtree(path)
            # Also exercise broken symlinks, which must fail the directory check.
            path.symlink_to(self.root / "nonexistent")
        success = missing is None
        self.run_setup(success=success)
        if success:
            self.check_links(asm, generic)
        else:
            self.assertFalse((self.package / "pizfix/os-include").exists())
    return test


def invalid_header_case(target, directory, kind):
    def test(self):
        if kind == "unreadable" and os.geteuid() == 0:
            self.skipTest("root can read files regardless of their permission bits")
        self.generate(target, target)
        self.layout(target, "flat")
        path = self.usr / "include" / directory / "types.h"
        if kind == "directory":
            path.unlink()
            path.mkdir()
        elif kind == "empty":
            path.write_text("")
        else:
            path.chmod(0)
        self.run_setup(success=False)
        self.assertFalse((self.package / "pizfix/os-include").exists())
    return test


def source_setup_case(target, host, layout, missing=False):
    def test(self):
        self.tool("uname", "echo " + host)
        asm, generic = self.layout(target, layout)
        script = self.root / "build_os_include.sh"
        script.write_text((source.parent / "build_os_include.sh").read_text().replace(
            "/usr/", str(self.usr) + "/"))
        self.package = self.root
        # No arguments is the normal source-build entry point. The binary
        # installer passes its baked-in target when embedding this script.
        args = [] if host in (target, "amd64", "arm64") else [target]
        result = subprocess.run(["sh", str(script), *args], cwd=self.root,
                                env=self.env, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.check_links(asm, generic)
        if missing:
            # A failed rerun must neither succeed nor remove existing links.
            before = {p.name: p.readlink() for p in (self.root / "pizfix/os-include").iterdir()}
            shutil.rmtree(asm)
            result = subprocess.run(["sh", str(script), *args], cwd=self.root,
                                    env=self.env, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0, result.stderr)
            self.assertIn("error: missing " + target + " kernel headers", result.stderr)
            after = {p.name: p.readlink() for p in (self.root / "pizfix/os-include").iterdir()}
            self.assertEqual(before, after)
    return test


for target, alias, other in (("x86_64", "amd64", "aarch64"),
                             ("aarch64", "arm64", "x86_64")):
    for host in (target, alias, other):
        for layout in ("flat", "multiarch", "cross-package"):
            name = f"test_{target}_on_{host}_{layout.replace('-', '_')}"
            setattr(PackageSetupTest, name, header_case(target, host, layout))
    for layout in ("flat", "multiarch", "cross-package"):
        for missing in ("asm", "linux", "asm-generic"):
            host = other if layout == "cross-package" else target
            name = f"test_{target}_{layout}_{missing}_missing".replace("-", "_")
            setattr(PackageSetupTest, name, header_case(target, host, layout, missing))
    for missing in ("asm", "linux", "asm-generic"):
        name = f"test_{target}_{missing}_broken_file".replace("-", "_")
        setattr(PackageSetupTest, name, header_case(target, target, "flat", missing, True))
    for directory in ("asm", "linux", "asm-generic"):
        for kind in ("directory", "empty", "unreadable"):
            name = f"test_{target}_{directory}_{kind}_header".replace("-", "_")
            setattr(PackageSetupTest, name, invalid_header_case(target, directory, kind))
    for host in (target, alias):
        for layout in ("flat", "multiarch"):
            name = f"test_source_{target}_on_{host}_{layout}"
            setattr(PackageSetupTest, name, source_setup_case(target, host, layout, True))
    setattr(PackageSetupTest, f"test_source_{target}_cross_package",
            source_setup_case(target, other, "cross-package", True))


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
