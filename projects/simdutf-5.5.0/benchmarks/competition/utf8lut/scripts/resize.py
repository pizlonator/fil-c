import sys, os

filename = sys.argv[1]
want_size = int(sys.argv[2])

buff_size = 2**20
remains = want_size
with open(filename + '.tmp', 'wb') as fo:
    while remains > 0:
        with open(filename, 'rb') as fi:
            while remains > 0:
                chunk = fi.read(buff_size)
                if len(chunk) == 0:
                    break
                if len(chunk) > remains:
                    chunk = chunk[:remains]
                fo.write(chunk)
                remains -= len(chunk)

os.replace(filename + '.tmp', filename)