"""Protect parallel failure propagation and serial benchmark execution."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import run_native_suites as runner


class NativeRunnerTests(unittest.TestCase):
    def test_failure_is_not_hidden_and_all_suites_finish(self):
        with tempfile.TemporaryDirectory() as directory:
            suites = [dict(name='pass', commands=['echo good']),
                      dict(name='fail', commands=['echo bad; exit 7', 'echo unreachable']),
                      dict(name='after', commands=['echo still-ran'])]
            with patch.dict('os.environ', {'GITHUB_STEP_SUMMARY': ''}):
                self.assertEqual(runner.run_all(suites, 2, Path(directory)), 1)
            self.assertIn('bad', (Path(directory) / 'fail.log').read_text())
            self.assertNotIn('unreachable', (Path(directory) / 'fail.log').read_text())
            self.assertIn('still-ran', (Path(directory) / 'after.log').read_text())

    def test_benchmark_runs_after_parallel_suites(self):
        finished = []
        def fake_run(suite, directory):
            if suite['name'] == 'pad_refresh_bench':
                self.assertEqual(set(finished), {'a', 'b'})
            finished.append(suite['name'])
            return suite['name'], 0, 0, directory / suite['name']
        with tempfile.TemporaryDirectory() as directory, \
                patch.object(runner, 'run_suite', side_effect=fake_run), \
                patch.dict('os.environ', {'GITHUB_STEP_SUMMARY': ''}):
            self.assertEqual(runner.run_all([{'name': n} for n in
                                            ['a', 'pad_refresh_bench', 'b']],
                                           2, Path(directory)), 0)
        self.assertEqual(finished[-1], 'pad_refresh_bench')

    def test_manifest_has_unique_executable_paths(self):
        suites = json.loads((runner.ROOT / 'scripts/native_suites.json').read_text())
        self.assertEqual(len(suites), len({s['name'] for s in suites}))
        for suite in suites:
            self.assertIn('-o /tmp/' + suite['name'], suite['commands'][0])
            self.assertTrue(suite['commands'][1].startswith('/tmp/' + suite['name']))


if __name__ == '__main__':
    unittest.main()
