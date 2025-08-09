import sys
from pathlib import Path
from table import Table


class Input:
    def __init__(self):
        self.procedure = None
        self.input_size = None
        self.iterations = None
        self.dataset = None

    @property
    def dataset_name(self):
        if self.dataset:
            return self.dataset.stem
        else:
            return 'none'

    def __str__(self):
        return '<Input: %s, size: %d, iterations: %s, dataset: %s>' % \
                (self.procedure, self.input_size, self.iterations, self.dataset)

    __repr__ = __str__


class Result:
    def __init__(self):
        self.instruction_per_byte = None
        self.instruction_per_cycle = None
        self.speed_gbs = None
        self.branch_misses = None
        self.cache_misses = None

    def __str__(self):
        return '<Result: %f ins/byte, %f ins/cycle, %f GB, %f b.misses/byte %f c.misses/byte>' % \
                (self.instruction_per_byte, self.instruction_per_cycle,
                 self.speed_gbs, self.branch_misses, self.cache_misses)

    __repr__ = __str__


def parse(file):
    result = []
    for line in file:
        for item in parse_line(line):
            if isinstance(item, Input):
                result.append(item)
            else:
                assert isinstance(result[-1], Input)
                result[-1] = (result[-1], item)

    return result


def parse_line(line):
    if 'input size' in line:
        yield parse_input(normalize_line(line))
    elif 'ins/byte' in line:
        yield parse_result(normalize_line(line))


def normalize_line(line):
    line = line.replace(',', ' ')
    line = line.replace(':', ' ')
    line = line.replace('(', ' ')
    line = line.replace(')', ' ')

    return line.split()


def parse_input(fields):
    input = Input()
    input.procedure = fields.pop(0)

    assert fields.pop(0) == 'input'
    assert fields.pop(0) == 'size'
    input.input_size = int(fields.pop(0))

    assert fields.pop(0) == 'iterations'
    input.iterations = int(fields.pop(0))

    try:
        assert fields.pop(0) == 'dataset'
        input.dataset = Path(fields.pop(0))
    except IndexError:
        pass

    return input


def parse_result(fields):
    result = Result()

    result.instruction_per_byte = float(fields.pop(0))
    assert fields.pop(0) == 'ins/byte'

    fields.pop(0)
    fields.pop(0)

    result.speed_gbs = float(fields.pop(0))
    assert fields.pop(0) == 'GB/s'

    fields.pop(0)
    fields.pop(0)

    result.instruction_per_cycle = float(fields.pop(0))
    assert fields.pop(0) == 'ins/cycle'

    result.branch_misses = float(fields.pop(0))
    assert fields.pop(0) == 'b.misses/byte'

    result.cache_misses = float(fields.pop(0))
    assert fields.pop(0) == 'c.mis/byte'

    return result


def print_speed_comparison(data):
    procedures = set()
    datasets = set()
    results = {}

    for input, result in data:
        procedures.add(input.procedure)
        datasets.add(input.dataset_name)
        results[(input.procedure, input.dataset_name)] = result

    datasets = list(sorted(datasets))
    procedures = list(sorted(procedures))

    def by_procedure():
        table = Table()
        table.set_header(['procedure'] + datasets)

        for procedure in procedures:
            row = []
            row.append(procedure)
            for dataset in datasets:
                try:
                    result = results[(procedure, dataset)]
                    row.append('%0.3f GB/s' % (result.speed_gbs))
                except KeyError:
                    row.append('--')

            table.add_row(row)

        return table

    def by_dataset():
        table = Table()
        table.set_header(['dataset'] + procedures)

        for dataset in datasets:
            row = []
            row.append(dataset)
            for procedure in procedures:
                try:
                    result = results[(procedure, dataset)]
                    row.append('%0.3f GB/s' % (result.speed_gbs))
                except KeyError:
                    row.append('--')

            table.add_row(row)

        return table

    if len(procedures) >= len(datasets):
        print(by_procedure())
    else:
        print(by_dataset())


def main():
    if len(sys.argv) < 2:
        print("No input files")
        print("Provide output from the benchmark utility")
        return

    for path in sys.argv[1:]:
        with open(path, 'rt') as f:
            data = parse(f)
            print_speed_comparison(data)


if __name__ == '__main__':
    main()
