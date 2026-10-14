"""Exercise independent clients against a fake single-slot dev channel."""
import fcntl
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1] / 'tools'
sys.path.insert(0, str(TOOLS))
import cssx


class RequestChannel(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='cssx-channel-')
        self.addCleanup(self.temp.cleanup)
        self.game = Path(self.temp.name)
        mod = cssx.mod_root(self.game)
        (mod / 'dev').mkdir(parents=True)
        (mod / 'dev/enabled.txt').touch()
        self.runtime = mod / 'runtime'
        self.runtime.mkdir()

    def client(self, label):
        code = ('import sys; from pathlib import Path; '
                f'sys.path.insert(0, {str(TOOLS)!r}); import cssx; '
                'print(__import__("json").dumps(cssx.request(Path(sys.argv[1]), '
                '{"op": sys.argv[2]}, timeout=5)))')
        proc = subprocess.Popen([sys.executable, '-c', code, str(self.game), label],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        def finish():
            if proc.poll() is None:
                proc.kill()
            proc.communicate()
        self.addCleanup(finish)
        return proc

    def test_processes_cannot_replace_an_unanswered_request(self):
        first = self.client('first')
        one = cssx.wait_json(self.runtime / 'request.json', lambda j: j['op'] == 'first', 5)
        second = self.client('second')
        time.sleep(0.3)
        self.assertEqual(json.loads((self.runtime / 'request.json').read_text()), one)
        cssx.atomic(self.runtime / 'response.json', one)
        two = cssx.wait_json(self.runtime / 'request.json', lambda j: j['op'] == 'second', 5)
        self.assertNotEqual(one['id'], two['id'])
        cssx.atomic(self.runtime / 'response.json', two)
        for proc, expected in ((first, one), (second, two)):
            stdout, stderr = proc.communicate(timeout=5)
            self.assertEqual(proc.returncode, 0, stderr)
            self.assertEqual(json.loads(stdout), expected)

    def test_lock_timeout_does_not_write_request(self):
        with (self.runtime / 'request.lock').open('a') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            with self.assertRaisesRegex(TimeoutError, 'No request slot'):
                cssx.request(self.game, {'op': 'blocked'}, timeout=0.1)
        self.assertFalse((self.runtime / 'request.json').exists())

    def test_response_timeout_releases_lock(self):
        with self.assertRaisesRegex(TimeoutError, 'No matching acknowledgement'):
            cssx.request(self.game, {'op': 'unanswered'}, timeout=0.1)
        with (self.runtime / 'request.lock').open('a') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)

    def test_write_error_releases_lock(self):
        with patch.object(cssx, 'atomic', side_effect=OSError('disk full')):
            with self.assertRaisesRegex(OSError, 'disk full'):
                cssx.request(self.game, {'op': 'failed'}, timeout=0.1)
        with (self.runtime / 'request.lock').open('a') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)


if __name__ == '__main__':
    unittest.main()
