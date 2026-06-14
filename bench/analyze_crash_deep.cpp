// Detailed parse of crash file
#include <cstdint>
#include <cstdio>
#include <vector>
#include <fstream>

int main() {
    std::ifstream f("../fuzz/crash-1909d956d6d9d6a7fa8a4145a9532469e499d0cc", std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    printf("Crash file size: %zu bytes\n\n", data.size());
    printf("%-4s %-10s %-12s %-12s\n", "Op#", "Code/Op", "Key", "Value");
    printf("------------------------------------------------\n");

    size_t i = 0;
    int n = 0;
    while (i + 9 <= data.size()) {
        uint8_t code = data[i] % 10;
        int32_t key = *reinterpret_cast<int32_t*>(&data[i+1]);
        int32_t val = *reinterpret_cast<int32_t*>(&data[i+5]);
        const char* names[] = {"INSERT", "ERASE", "FIND", "ACCESS", "ITERATE", "CLEAR", "RESERVE", "INSASGN", "ERASEIT", "COUNT"};
        printf("%3d: code=%u %-10s key=0x%08X(%d) val=0x%08X(%d)\n",
               n, data[i], names[code], key, key, val, val);
        i += 9;
        n++;
    }
    return 0;
}
