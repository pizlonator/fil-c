import argparse
import os
import sys

try:
    from lxml import etree
except ImportError:
    # Skip the test if lxml is not available
    sys.exit(77)


args = argparse.ArgumentParser(prog="validate-gir")
args.add_argument("-S", "--schema", metavar="FILE", help="the schema file", required=True)
args.add_argument("infile", metavar="GIRFILE", type=argparse.FileType('r', encoding='UTF-8'),
                  default=sys.stdin, help="the GIR file to validate")

options = args.parse_args(sys.argv[1:])

with open(options.schema, "r", encoding="utf-8") as f:
    schema = etree.RelaxNG.from_rnc_string(f.read())

gir = etree.parse(options.infile)
if not schema.validate(gir):
    print(f"Validation errors: {schema.error_log}", file=sys.stderr)
    sys.exit(1)
