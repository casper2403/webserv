#!/usr/bin/python3
import os
import sys

# 1. Send Headers (Must end with blank line)
print("Content-Type: text/html\r\n\r\n")

# 2. Send Body
print("<html><body>")
print("<h1>CGI Test Script (Python)</h1>")

# Print Environment Variables passed by Webserver
print("<h2>Environment Variables</h2>")
print("<ul>")
for k, v in os.environ.items():
    print(f"<li><b>{k}:</b> {v}</li>")
print("</ul>")

# Print Body passed via Stdin (for POST requests)
print("<h2>Request Body</h2>")
print("<pre>")
try:
    # Safe Content-Length parsing
    content_length_str = os.environ.get("CONTENT_LENGTH", "")
    length = 0
    
    if content_length_str:
        try:
            length = int(content_length_str)
        except ValueError:
            print(f"Invalid Content-Length received: '{content_length_str}'")
    
    if length > 0:
        # Read exactly 'length' bytes from stdin
        body = sys.stdin.read(length)
        print(f"Read {length} bytes: {body}")
    else:
        print("No body content (Content-Length is 0 or missing).")

except Exception as e:
    print(f"Error reading body: {e}")
print("</pre>")

print("</body></html>")