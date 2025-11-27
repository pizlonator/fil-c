import io
import os
import tempfile
import difflib
import contextlib
import subprocess
import unittest


@contextlib.contextmanager
def temp_filename(*args, **kwargs):
    fd, name = tempfile.mkstemp(*args, **kwargs)
    try:
        os.close(fd)
        yield name
    finally:
        os.unlink(name)


class TestOffsets(unittest.TestCase):

    def test_main(self):
        exe = ".exe" if os.name == "nt" else ""
        gitestoffsets = os.path.join(os.environ["builddir"], "gitestoffsets" + exe)

        env = os.environ.copy()
        env["GI_TYPELIB_PATH"] = os.pathsep.join(
            [os.environ["builddir"], os.environ["top_builddir"]])

        with temp_filename(suffix=".compiled") as compiled_name, \
                temp_filename(suffix=".introspected") as introspected_name:

            subprocess.check_call(
                [gitestoffsets, compiled_name, introspected_name], env=env)

            with io.open(compiled_name, encoding="utf-8") as compiled, \
                    io.open(introspected_name, encoding="utf-8") as introspected:

                result = difflib.unified_diff(
                    compiled.readlines(), introspected.readlines(),
                    fromfile=compiled_name, tofile=introspected_name)
                result = "".join(result)
                if result:
                    raise AssertionError(result)


if __name__ == '__main__':
    unittest.main()
