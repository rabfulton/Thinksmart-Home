#!/usr/bin/env python3
"""Local WARP readiness tests: no WARP, network, microphone or cloud required."""
import importlib.util
from pathlib import Path
import subprocess
import os
import socket
import threading
import time
import unittest
from unittest.mock import MagicMock

spec = importlib.util.spec_from_file_location("ready", Path(__file__).with_name("thinksmart-warp-ready.py"))
ready = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ready)


class ReadinessTests(unittest.TestCase):
    def exercise(self, states, backend=True, ipc_failures=0):
        elapsed = [0.0]
        calls = []
        statuses = iter(states)
        failures = [ipc_failures]

        def run(command, **kwargs):
            calls.append(command[-1])
            if command[-1] == "connect":
                if failures[0]:
                    failures[0] -= 1
                    raise subprocess.TimeoutExpired(command, 2)
                return subprocess.CompletedProcess(command, 0)
            return subprocess.CompletedProcess(command, 0, next(statuses, "Status update: Connecting"))

        def connect(address, **kwargs):
            self.assertEqual(address, ("127.0.0.1", 40001))
            if not backend:
                raise ConnectionRefusedError()
            return MagicMock()

        def sleep(seconds):
            elapsed[0] += seconds

        result = ready.wait_ready(timeout=1, run=run, connect=connect,
                                  clock=lambda: elapsed[0], sleep=sleep)
        return result, calls, elapsed[0]

    def test_waits_for_connected_and_listening(self):
        result, calls, _ = self.exercise(["Status update: Connecting", "Status update: Connected\nNetwork: healthy"])
        self.assertTrue(result)
        self.assertEqual(calls, ["connect", "status", "status"])

    def test_disconnected_does_not_match_connected(self):
        self.assertFalse(self.exercise(["Status update: Disconnected"])[0])

    def test_connected_without_backend_fails_bounded(self):
        result, _, elapsed = self.exercise(["Status update: Connected"] * 5, backend=False)
        self.assertFalse(result)
        self.assertEqual(elapsed, 1)

    def test_retries_early_daemon_ipc_failure(self):
        result, calls, _ = self.exercise(["Status update: Connected"], ipc_failures=1)
        self.assertTrue(result)
        self.assertEqual(calls, ["connect", "connect", "status"])

    def test_unavailable_tunnel_fails_bounded_without_reconnecting_loop(self):
        result, calls, elapsed = self.exercise([])
        self.assertFalse(result)
        self.assertEqual(calls.count("connect"), 1)
        self.assertEqual(elapsed, 1)


@unittest.skipUnless(Path('/usr/lib/systemd/systemd-socket-proxyd').exists(), 'systemd proxy unavailable')
class NativeProxyTests(unittest.TestCase):
    def test_socket_activation_idle_exit_and_open_connection_retention(self):
        with socket.socket() as backend, socket.socket() as frontend:
            backend.bind(('127.0.0.1', 0))
            backend.listen()
            backend.settimeout(5)
            frontend.bind(('127.0.0.1', 0))
            frontend.listen()
            frontend.set_inheritable(True)
            errors = []

            def echo():
                try:
                    for _ in range(2):
                        conn, _ = backend.accept()
                        with conn:
                            while data := conn.recv(4096):
                                conn.sendall(data)
                except Exception as error:
                    errors.append(error)

            worker = threading.Thread(target=echo, daemon=True)
            worker.start()
            # PID1 normally supplies fd 3. This child wrapper supplies the same
            # activation contract without needing a running systemd in tests.
            wrapper = ('import os,sys; os.dup2(int(sys.argv[1]),3); os.set_inheritable(3,True); '
                       'os.environ.update(LISTEN_PID=str(os.getpid()),LISTEN_FDS="1"); '
                       'os.execv(sys.argv[2],sys.argv[2:])')
            for _ in range(2):
                with subprocess.Popen([os.sys.executable, '-c', wrapper, str(frontend.fileno()),
                                       '/usr/lib/systemd/systemd-socket-proxyd',
                                       '--exit-idle-time=0.2s',
                                       '127.0.0.1:' + str(backend.getsockname()[1])],
                                      pass_fds=(frontend.fileno(),), stdout=subprocess.DEVNULL,
                                      stderr=subprocess.PIPE) as process:
                    with socket.create_connection(frontend.getsockname(), timeout=3) as client:
                        client.sendall(b'archive fixture')
                        self.assertEqual(client.recv(4096), b'archive fixture')
                        time.sleep(0.4)
                        self.assertIsNone(process.poll(), 'An open, idle media tunnel must stay alive')
                        client.sendall(b'resume')
                        self.assertEqual(client.recv(4096), b'resume')
                    self.assertEqual(process.wait(timeout=3), 0)
            worker.join(timeout=5)
            self.assertFalse(worker.is_alive())
            self.assertEqual(errors, [])


if __name__ == "__main__":
    unittest.main()
