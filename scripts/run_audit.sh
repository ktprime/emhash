cd /mnt/d/emhash
g++ -std=c++17 -O2 -I include -o /tmp/test_audit tests/verify/test_lifecycle_audit.cpp 2>&1
echo "BUILD_EXIT=$?"
/tmp/test_audit 2>&1
echo "RUN_EXIT=$?"