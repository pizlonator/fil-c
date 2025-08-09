#!/usr/bin/env python3

from pathlib import Path

def main():
    def input_files():
        for path in Path('.').glob('*.*'):
            if path.suffix in ('.html', '.txt'):
                yield path
        
    for path in input_files():
        text = path.read_text(encoding='utf8')

        dstpath = path.parent / (path.name + '.utf16')
        print("Writing %s" % dstpath)
        dstpath.write_text(text, encoding='utf16')


if __name__ == '__main__':
    main()
