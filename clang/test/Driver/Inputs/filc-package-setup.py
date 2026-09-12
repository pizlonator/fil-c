import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


# Exercise only setup.sh generation and header symlinks, without packaging or
# building anything. Replace /usr in the generated script with a temporary tree.
source = Path(sys.argv[1]).read_text()
start = source.index('echo "target_arch=$ARCH" >> setup.sh')
end = source.index("\nEOF\n", start) + len("\nEOF\n")
generator = source[start:end]


class PackageSetupTest(unittest.TestCase):
    def check_layout(self, target, host, layout):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            usr = root / "usr"
            generic = usr / "include"
            asm = None
            if layout == "multiarch":
                asm = generic / (target + "-linux-gnu/asm")
            elif layout == "cross-package":
                generic = usr / (target + "-linux-gnu/include")
                asm = generic / "asm"
            elif layout == "native":
                asm = generic / "asm"
            for name in ("linux", "asm-generic"):
                (generic / name).mkdir(parents=True)
            if asm:
                asm.mkdir(parents=True)
            # Foreign hosts may have flat host asm headers. Never select them.
            if target != host:
                (usr / "include/asm").mkdir(parents=True, exist_ok=True)
            (root / "pizfix").mkdir()
            tools = root / "tools"
            tools.mkdir()
            uname = tools / "uname"
            uname.write_text("#!/bin/sh\necho " + host + "\n")
            uname.chmod(0o755)
            env = dict(os.environ, ARCH=target, PATH=str(tools) + os.pathsep + os.environ["PATH"])
            subprocess.run(["sh", "-ec", generator], cwd=root, env=env, check=True)
            setup = (root / "setup.sh").read_text().replace("/usr/", str(usr) + "/")
            subprocess.run(["sh", "-n"], input=setup, text=True, check=True)
            result = subprocess.run(
                ["sh", "-ec", setup], cwd=root, env=env, text=True,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True,
            )
            headers = root / "pizfix/os-include"
            for name in ("linux", "asm-generic"):
                self.assertEqual((headers / name).readlink(), generic / name)
            if asm:
                self.assertEqual((headers / "asm").readlink(), asm)
                self.assertNotIn("warning:", result.stderr)
            else:
                self.assertFalse((headers / "asm").is_symlink())
                self.assertIn("warning: no " + target + " kernel headers", result.stderr)

    def test_header_layouts_in_both_directions(self):
        for target in ("x86_64", "aarch64"):
            other = "aarch64" if target == "x86_64" else "x86_64"
            for host in (target, other):
                for layout in ("multiarch", "cross-package", "missing"):
                    with self.subTest(target=target, host=host, layout=layout):
                        self.check_layout(target, host, layout)
            with self.subTest(target=target, layout="native"):
                self.check_layout(target, target, "native")


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
