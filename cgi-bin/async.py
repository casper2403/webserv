#!/usr/bin/python3
import time
import os
import sys

# Debug log
with open("cgi_debug.log", "a") as f:
    f.write(f"Script started PID: {os.getpid()}\n")

print("Content-Type: text/plain\r\n\r\n", end="")
sys.stdout.flush()

print("Start Sleep...")
sys.stdout.flush()

time.sleep(5) # <--- If this runs, the browser MUST spin for 5s

print("Wake up!")