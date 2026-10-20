"""Run every native suite with bounded parallelism and readable per-suite logs."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
import os
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def run_suite(suite, log_dir):
    start = time.monotonic()
    path = log_dir / (suite['name'] + '.log')
    env = dict(os.environ)
    env.setdefault('HB_TEST_CC', 'clang')
    with path.open('w') as log:
        result = subprocess.run(['bash', '-e', '-o', 'pipefail', '-c',
                                 '\n'.join(suite['commands'])],
                                cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT)
    elapsed = time.monotonic() - start
    return suite['name'], result.returncode, elapsed, path


def run_all(suites, workers, log_dir):
    log_dir.mkdir(parents=True, exist_ok=True)
    results = []

    def report(result):
        name, code, elapsed, path = result
        results.append(result)
        print(f'{"PASS" if code == 0 else "FAIL"} {name}: {elapsed:.1f}s', flush=True)
        if code:
            print(path.read_text(), flush=True)

    # Wall-clock performance assertions must not compete with other suites.
    isolated = [s for s in suites if s['name'] == 'pad_refresh_bench']
    parallel = [s for s in suites if s['name'] != 'pad_refresh_bench']
    with ThreadPoolExecutor(max_workers=workers) as pool:
        for future in as_completed([pool.submit(run_suite, s, log_dir) for s in parallel]):
            report(future.result())
    for suite in isolated:
        report(run_suite(suite, log_dir))
    summary = '\n'.join(f'{name}: {elapsed:.1f}s ({"PASS" if code == 0 else "FAIL"})'
                        for name, code, elapsed, _ in sorted(results, key=lambda r: -r[2]))
    print('\nNative suite durations (slowest first):\n' + summary, flush=True)
    if os.environ.get('GITHUB_STEP_SUMMARY'):
        with open(os.environ['GITHUB_STEP_SUMMARY'], 'a') as output:
            output.write('### Native suite durations\n\n```text\n' + summary + '\n```\n')
    return int(any(code for _, code, _, _ in results))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jobs', type=int, default=min(4, os.cpu_count() or 1))
    parser.add_argument('--logs', type=Path, default=Path('/tmp/hb-native-logs'))
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    suites = json.loads((ROOT / 'scripts/native_suites.json').read_text())
    names = [suite['name'] for suite in suites]
    if len(names) != len(set(names)):
        raise ValueError('suite names and executable paths must be unique')
    raise SystemExit(run_all(suites, args.jobs, args.logs))


if __name__ == '__main__':
    main()
