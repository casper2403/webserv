#!/usr/bin/python3
import time
import os
import sys

# Send headers
print("Content-Type: text/plain\r\n\r\n", end="")
sys.stdout.flush() # Force write to pipe immediately

print("Start Sleep (PID: {})...".format(os.getpid()))
sys.stdout.flush()

time.sleep(5) # The server must NOT block other clients during this

print("Wake up! Done.")