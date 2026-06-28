#include <cstdint>
#include <cstdio>
int main() {
    uint32_t inactive = 0xFFFFFFFFu;
    printf("static_cast<uint32_t>(INACTIVE) < 0 = %d\n", (int)(static_cast<uint32_t>(inactive) < 0));
    printf("static_cast<int>(INACTIVE) < 0 = %d\n", (int)(static_cast<int>(inactive) < 0));
    printf("INACTIVE == 0xFFFFFFFFu = %d\n", (int)(inactive == 0xFFFFFFFFu));
    return 0;
}
