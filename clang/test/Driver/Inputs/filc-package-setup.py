import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest


# Generate a complete installer; stub ELF mutations but run setup commands.
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
        files = {
            "libpas/common.sh": f"OS=linux\nARCH={target}\n",
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

    def layout(self):
        for name in ("asm", "linux", "asm-generic"):
            path = self.usr / "include" / name
            path.mkdir(parents=True)
            (path / "types.h").write_text("/* header fixture */\n")

    def run_setup(self):
        result = subprocess.run(
            ["sh", "setup.sh"], cwd=self.package, env=self.env, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("You are all set", result.stdout)
        self.assertTrue((self.root / "patchelf.log").exists())
        return result

    def test_relocation_distinguishes_executables_libraries_and_scripts(self):
        self.generate("x86_64", "x86_64")
        self.layout()
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

    def test_relocation_failure_does_not_report_success(self):
        self.generate("x86_64", "x86_64")
        self.layout()
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


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
