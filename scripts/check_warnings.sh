cd /mnt/d/emhash
for f in tests/verify/*.cpp tests/stress/*.cpp tests/attack/*.cpp tests/debug/*.cpp; do
    base=$(basename "$f")
    echo "=== $base ==="
    g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Wno-unused-parameter -I. -Ithirdparty "$f" -o /tmp/test_xxx 2>&1 | grep "$base" | head -10
done