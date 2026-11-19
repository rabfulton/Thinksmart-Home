#!/usr/bin/env python3
"""Wait for the separately installed WARP proxy; never change routes or identity."""
# SPDX-License-Identifier: MIT
import socket
import subprocess
import sys
import time


def wait_ready(timeout=18, run=subprocess.run, connect=socket.create_connection,
               clock=time.monotonic, sleep=time.sleep):
    deadline = clock() + timeout
    requested = False
    while clock() < deadline:
        try:
            remaining = max(0.05, min(2, deadline - clock()))
            if not requested:
                result = run(["/usr/bin/warp-cli", "--accept-tos", "connect"],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                             timeout=remaining, check=False)
                requested = result.returncode == 0
            if requested and clock() < deadline:
                result = run(["/usr/bin/warp-cli", "--accept-tos", "status"],
                             capture_output=True, text=True,
                             timeout=max(0.05, min(2, deadline - clock())), check=False)
                if result.returncode == 0 and "Status update: Connected" in result.stdout:
                    with connect(("127.0.0.1", 40001), timeout=0.25):
                        return True
        except (OSError, subprocess.TimeoutExpired):
            pass
        sleep(min(0.25, max(0, deadline - clock())))
    return False


if __name__ == "__main__":
    if not wait_ready():
        print("Archive proxy unavailable: WARP did not become ready in time.", file=sys.stderr)
        sys.exit(1)
