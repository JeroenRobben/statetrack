#!/usr/bin/env python3
"""
Corpus runner for extract_value_names.py.

For each <case>/ with an expected.json: generate a hermetic compile_commands.json
(every *.c in the dir, with -I<statetrack> so the STATETRACK_REGISTER macros
resolve), run the tool, and diff normalized JSON.

Run:  python3 run.py   (or `make check` from statetrack/).
Stdlib only; runs under a python whose clang.cindex works (system python3).
"""

import glob
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))          # statetrack/extract_value_names_tests
STATETRACK = os.path.abspath(os.path.join(HERE, ".."))     # has statetrack_annotate.h
TOOL = os.path.join(STATETRACK, "extract_value_names.py")


def _resource_include():
    """clang's builtin-header dir (stddef.h etc.); libclang needs it explicitly."""
    try:
        rd = subprocess.run(["clang", "-print-resource-dir"],
                            capture_output=True, text=True).stdout.strip()
        if rd:
            return ["-isystem", os.path.join(rd, "include")]
    except Exception:
        pass
    return []


RESOURCE = _resource_include()


def gen_compile_db(fixture):
    cfiles = sorted(os.path.basename(f) for f in glob.glob(os.path.join(fixture, "*.c")))
    db = [{"directory": fixture,
           "file": f,
           "arguments": ["clang", "-std=c11", "-I" + STATETRACK] + RESOURCE + ["-c", f]}
          for f in cfiles]
    path = os.path.join(fixture, "compile_commands.json")
    with open(path, "w") as fh:
        json.dump(db, fh, indent=2)
    return path


def norm(obj):
    return json.dumps(obj, indent=2, sort_keys=True)


def run_fixture(fixture):
    db = gen_compile_db(fixture)
    r = subprocess.run([sys.executable, TOOL, "--compile-db", db],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return False, "tool failed:\n" + r.stderr.strip()
    got = json.loads(r.stdout)
    exp = json.load(open(os.path.join(fixture, "expected.json")))
    if norm(got) == norm(exp):
        return True, ""
    return False, "--- expected ---\n%s\n--- got ---\n%s" % (norm(exp), norm(got))


def main():
    fixtures = sorted(d for d in glob.glob(os.path.join(HERE, "*"))
                      if os.path.isdir(d) and os.path.exists(os.path.join(d, "expected.json")))
    npass = 0
    for fx in fixtures:
        ok, msg = run_fixture(fx)
        print("  %s  %s" % ("PASS" if ok else "FAIL", os.path.basename(fx)))
        if not ok:
            print("\n".join("    " + ln for ln in msg.splitlines()))
        npass += ok
    print("%d/%d decode fixtures passed" % (npass, len(fixtures)))
    sys.exit(0 if npass == len(fixtures) else 1)


if __name__ == "__main__":
    main()
