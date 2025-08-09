import subprocess, os, sys, re, shutil, json

RunsCount = 10000

ConvertToUtf16 = "FileConverter_msvc %s %s"

Solutions_Decode = {
    "trivial":    "FileConverter_msvc %s temp.out -b=0 -k={0} --small",
    "utf8lut:1S": "FileConverter_msvc %s temp.out -b=3 -k={0} --small",
    "utf8lut:4S": "FileConverter_msvc %s temp.out -b=3 -k={0}",
    "u8u16": "u8u16_ssse3 %s temp.out {0}",
    "utf8sse4": "utf8sse4 %s temp.out {0}",
}
Solutions_Encode = {
    "trivial":    "FileConverter_msvc -s=utf16 -d=utf8 %s temp.out -b=0 -k={0} --small",
    "utf8lut:1S": "FileConverter_msvc -s=utf16 -d=utf8 %s temp.out -b=3 -k={0} --small",
    "utf8lut:4S": "FileConverter_msvc -s=utf16 -d=utf8 %s temp.out -b=3 -k={0}",
}

encode = (sys.argv[1] == 'encode')
Solutions = (Solutions_Encode if encode else Solutions_Decode)
for k,v in Solutions.items():
    Solutions[k] = v.format(RunsCount)

Tests = {
    "[rnd1111:400000]": "rnd1111_utf8.txt",
    "[rnd1110:500000]": "rnd1110_utf8.txt",
    "chinese": "chinese_book.txt",
    "russian": "war_and_piece.fb2",
    "english": "english_book.txt",
    "unicode": "unicode_table.html",
}

def log_name_of(sol_name, test_name):
    log_name = sol_name + "__" + test_name
    log_name = "logs/" + re.sub(r'[\W]', '_', log_name) + ".log"
    return log_name

def run_sol_test(sol_name, test_name, encode=False):
    if encode:
        subprocess.run((ConvertToUtf16 % (Tests[test_name], 'temp.in')).split(), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        cmd = Solutions[sol_name] % 'temp.in'
    else:
        cmd = Solutions[sol_name] % Tests[test_name]
    log_name = log_name_of(sol_name, test_name)
    with open(log_name, 'wt') as f:
        subprocess.run(cmd.split(), stdout=f, stderr=f)
    return log_name

def parse_log(log_name):
    with open(log_name, 'rt') as f:
        text = f.read()
    external_result = None
    internal_result = None
    # typical overall measurement, added to every solution by myself
    m = re.search(r'From total time\s+:\s+([\d.]+)\s+cyc/el\n', text)
    if m:
        external_result = float(m.group(1))
    # internal timings in utf8lut
    for m in re.finditer(r'slot \d+\s+(DECODE|ENCODE)\s+:\s+([\d.]+) cyc/el\s+(\d+) elems\n', text):
        if int(m.group(3)) > 0:
            internal_result = float(m.group(2))
    # internal timings in u8u16
    for m in re.finditer(r'BOM \d+: \d+ \(avg time: \d+ cyc/kElem\) Cumulative: \d+ \(avg: (\d+) cyc/kElem\)', text):
        internal_result = float(m.group(1)) / 1000.0
    return internal_result, external_result

def main():
    shutil.rmtree('logs', ignore_errors=True)
    os.mkdir('logs')

    #print(parse_log(run_sol_test("utf8lut:1S", "chinese")))
    #print(parse_log(run_sol_test("u8u16", "chinese")))
    #print(parse_log(run_sol_test("utf8sse4", "chinese")))

    jsonall = {}
    jsonall['solutions'] = list(Solutions.keys())
    jsonall['tests'] = list(Tests.keys())
    data = jsonall['xdata'] = {}
    for test in Tests:
        data[test] = {}
        for sol in Solutions:
            print("Test %s, Sol %s:" % (test, sol), end="", flush=True)
            log_name = run_sol_test(sol, test, encode)
            print(" finished: ", end="", flush=True)
            result = parse_log(log_name)
            print(result[0], '/', result[1], flush=True)
            data[test][sol] = (result[0], result[1], log_name)

    with open("logs/results.json", "wt") as f:
        json.dump(jsonall, f, indent=2, sort_keys=True)

if __name__ == "__main__":
    main()
