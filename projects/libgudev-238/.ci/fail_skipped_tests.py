#!/usr/bin/python3

from lxml import etree
import sys

def format_title(title):
    """Put title in a box"""
    box = {
        'tl': '╔', 'tr': '╗', 'bl': '╚', 'br': '╝', 'h': '═', 'v': '║',
    }
    hline = box['h'] * (len(title) + 2)

    return '\n'.join([
        f"{box['tl']}{hline}{box['tr']}",
        f"{box['v']} {title} {box['v']}",
        f"{box['bl']}{hline}{box['br']}",
    ])

tree = etree.parse(sys.argv[1])
for suite in tree.xpath('/testsuites/testsuite'):
    skipped = suite.get('skipped')
    if int(skipped) != 0:
        print(format_title('Tests were skipped when they should not have been. All the tests must be run in the CI'),
                end='\n\n', flush=True)
        sys.exit(1)
