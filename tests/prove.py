
import socket
import subprocess
import sys
import time
import re

def request(port, command):
    with socket.create_connection(
        ("127.0.0.1", port), timeout=5
    ) as s:
        s.settimeout(15)
        s.sendall((command + "\n").encode())

        data = b""
        while not data.endswith(b"\n"):
            chunk = s.recv(1024)
            if not chunk:
                break
            data += chunk

        return data.decode(errors="replace").strip()

def concurrency():
    print("Testing 10 concurrent clients...")

    start = time.monotonic()
    processes = []

    for i in range(10):
        p = subprocess.Popen(
            ["./bin/client", "--command", "SLEEP 6"],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True
        )
        processes.append(p)

    pids = []
    successes = 0

    for i, p in enumerate(processes, 1):
        output, _ = p.communicate(timeout=15)
        match = re.search(
            r"OK SLEEP pid=(\d+) seconds=6",
            output
        )

        if match and p.returncode == 0:
            pids.append(match.group(1))
            successes += 1

        print(f"Client {i}: {output.strip().splitlines()[-1]}")

    elapsed = time.monotonic() - start

    print(f"\nSuccessful clients: {successes}/10")
    print(f"Unique child PIDs: {len(set(pids))}")
    print(f"Elapsed time: {elapsed:.2f} seconds")

    if successes == 10 and len(set(pids)) == 10 and elapsed < 12:
        print("PASS: Concurrent processing confirmed")
    else:
        print("CHECK: Review concurrency results")

def idle_test(port):
    with socket.create_connection(
        ("127.0.0.1", port), timeout=5
    ) as s:
        s.settimeout(3.5)
        try:
            data = s.recv(1024)
            return data.decode(errors="replace").strip()
        except socket.timeout:
            return "No response after 3.5 seconds"

def robustness():
    print("TEST 1: Invalid Input")
    print("SAFE:", request(9090, "SLEEP 2abc"))
    print("UNSAFE:", request(9091, "SLEEP 2abc"))

    print("\nTEST 2: Oversized Request")
    long_command = "ECHO " + "X" * 300
    print("SAFE:", request(9090, long_command))
    print("UNSAFE:", request(9091, long_command)[:80], "...")

    print("\nTEST 3: Idle Client Timeout")
    print("SAFE:", idle_test(9090))
    print("UNSAFE:", idle_test(9091))

if __name__ == "__main__":
    try:
        if len(sys.argv) != 2:
            print("Use: python3 tests/prove.py concurrency|robustness")
        elif sys.argv[1] == "concurrency":
            concurrency()
        elif sys.argv[1] == "robustness":
            robustness()
        else:
            print("Unknown test")
    except (OSError, subprocess.TimeoutExpired) as e:
        print("Test error:", e)
        print("Check that the servers are running.")
