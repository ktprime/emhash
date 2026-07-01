#!/bin/bash
set -e
cd /mnt/d/emhash
g++ -std=c++17 -O2 -I include -o /tmp/test_leak tests/verify/test_string_key_leak.cpp 2>&1 | head -20
echo "--- compile done ---"
/tmp/test_leak 2>&1 | tail -3
