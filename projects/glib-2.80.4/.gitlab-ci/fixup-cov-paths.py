import sys
import os
import io
import re


def main(argv):
    # Fix paths in lcov files generated on a Windows host so they match our
    # current source layout.
    paths = argv[1:]

    for path in paths:
        print("cov-fixup:", path)
        text = io.open(path, "r", encoding="utf-8").read()
        text = text.replace("\\\\", "/")
        old_root = re.search(":(.*?)/glib/.*?$", text, re.MULTILINE).group(1)
        new_root = os.path.dirname(os.getcwd())
        if old_root != new_root:
            print("replacing %r with %r" % (old_root, new_root))
        text = text.replace(old_root, new_root)
        with io.open(path, "w", encoding="utf-8") as h:
            h.write(text)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
