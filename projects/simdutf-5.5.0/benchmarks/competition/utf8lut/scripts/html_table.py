import json, sys

with open(sys.argv[1] + "/results.json", 'rt') as f:
    data = json.load(f)

tests = data['tests']
solutions = data['solutions']

print('<tr>')
print('  <th></th>')
for sol in solutions:
    print('  <th>%s</th>' % sol)
print('</tr>')
for test in tests:
    print('<tr>')
    print('  <th>%s</th>' % test)
    for sol in solutions:
        values = data['xdata'][test][sol]
        for i in range(2):
            if values[i] is None:
                values[i] = '???'
            else:
                values[i] = '%.2f' % values[i]
        print('  <td>%s / %s</td>' % (values[0], values[1]))
    print('</tr>')
