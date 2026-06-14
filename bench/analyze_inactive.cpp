// Analyze INACTIVE key conflict hypothesis for emhash8
#include <cstdint>
#include <cstdio>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <cassert>
#include "../hash_table8.hpp"

enum OpCode : uint8_t {
    OP_INSERT = 0, OP_ERASE = 1, OP_FIND = 2,
    OP_ACCESS = 3, OP_ITERATE = 4, OP_CLEAR = 5,
    OP_RESERVE = 6, OP_INSERT_OR_ASSIGN = 7,
    OP_ERASE_ITERATOR = 8, OP_COUNT = 9,
};

struct Op { OpCode code; int key; int value; };

int main() {
    // Simulate the crash sequence: reserve(1) is the trigger
    emhash8::HashMap<int, int> em;

    // Sequence from crash file (data[0..15] in hex):
    // 0x29 0xff 0x20 0x00 0x00 0x00 0xef 0x00 0xed ...
    // op0: code=0x29%10=1 (ERASE), key=0x0020ff=8447
    // op1: code=0xed%10=5 (CLEAR)
    // op2: code=0x52%10=0 (INSERT), key=0x00020000=131072, val=0
    // op3: code=0xac%10=2 (FIND), key=394
    // op4: code=0xac%10=2 (FIND)... wait, let me re-parse

    // Actually code = data[i] % 10:
    // data[0]=0x29 -> 41%10=1 (ERASE)
    // data[9]=0x00 -> 0%10=0 (INSERT)
    // data[18]=0x68 -> 104%10=4 (ITERATE)
    // data[27]=0xac -> 172%10=2 (FIND)
    // data[36]=0x00 -> 0%10=0 (INSERT)
    // data[45]=0x68 -> 4 (ITERATE)
    // data[54]=0xac -> 2 (FIND)
    // data[63]=0x00 -> 0 (INSERT)
    // data[72]=0xff -> 5 (CLEAR)
    // data[81]=0x68 -> 4 (ITERATE)
    // data[90]=0x68 -> 4 (ITERATE)
    // data[99]=0x68 -> 4 (ITERATE)
    // data[108]=0xff -> 5 (CLEAR)
    // data[117]=0x27 -> 9 (COUNT)
    // data[126]=0xff -> 5 (CLEAR)

    // The key in last insert (data[72..76]): 0x68, 0x68, 0x68, 0x68 = 0x68686868 = 1751672936
    // This is 0x68686868 which has high bits 0x68 = 104

    // In the size_type (uint32_t), INACTIVE = 0xFFFFFFFF
    // _mask = num_buckets - 1
    // If num_buckets = 4, _mask = 3
    // hash high bits (key_hash & ~_mask) = 1751672936 & 0xFFFFFFFC = 1751672936

    // Actually let me check: 1751672936 = 0x68686868
    // 0x68686868 & ~3 = 0x68686868
    // The Index stores slot | (key_hash & ~_mask)
    // If slot=0, then _index.slot = 0x68686868

    // When we read _index[].next, which is int32_t:
    // If the high bit is set (>= 0x80000000), it's considered "negative" = EMPTY/INACTIVE
    // 0x68686868 is positive, so it should be a valid pointer

    // But wait - when the user calls find_or_allocate with this key:
    // 1. bucket = key_hash & _mask (low bits) - some small value
    // 2. next_bucket = _index[bucket].next
    // 3. If next_bucket is "negative" (high bit set) -> INACTIVE/empty slot

    // The crash: rdi = 0x7bd7162e01cc (an arbitrary high value)
    // This came from _index[].next, which means the chain had a "next" pointer
    // that points outside the array

    printf("Analyzing INACTIVE key conflict scenario...\n");
    printf("size_type INACTIVE = 0x%X\n", emhash8::HashMap<int,int>::INACTIVE);

    // Test: can a key value collide with INACTIVE marker?
    // For int32_t, INACTIVE = (uint32_t)(-1) = 0xFFFFFFFF
    // _index[].next is stored as int32_t
    // _index[].next == 0xFFFFFFFF means INACTIVE (next is "empty")

    // The "negative" check: (int)next_bucket < 0
    // In a 32-bit system, 0xFFFFFFFF cast to int = -1 < 0 -> INACTIVE
    // But for INACTIVE collision: a valid bucket index like 0x80000001 is also "negative"

    // Example: if _mask = 0xFF and num_buckets = 256, indices are 0-254
    // But if _index[].next = 0x80000001 (which is "negative" in int32)
    // The check (int)next < 0 returns true -> treated as INACTIVE
    // However, 0x80000001 is NOT a valid empty marker (INACTIVE = 0xFFFFFFFF)
    // This causes us to skip a valid chain entry

    printf("If _mask = 0xFF (num_buckets=256):\n");
    printf("  Index 0x80000001 cast to int = %d (treated as <0 = INACTIVE)\n",
           (int)0x80000001);
    printf("  But real INACTIVE = 0x%X = -1\n", emhash8::HashMap<int,int>::INACTIVE);

    // Hypothesis: when (int)next_bucket < 0 but next_bucket != INACTIVE
    // The code at line 1544-1547 does:
    //   if (next_bucket != INACTIVE) pop_empty(bucket);
    // This is for EMH_HIGH_LOAD, but otherwise the code just returns `bucket`
    // which means the slot at `bucket` is treated as empty even though it isn't!

    // This can cause:
    // 1. Two different keys to claim the same main_bucket
    // 2. Collision chain corruption
    // 3. Out-of-bounds access in subsequent operations

    printf("\nROOT CAUSE HYPOTHESIS:\n");
    printf("When _index[].next has the high bit set (>= 0x80000000) but is not\n");
    printf("exactly INACTIVE (0xFFFFFFFF), the code misinterprets it as an empty slot.\n");
    printf("This causes the chain to be broken, and subsequent operations can\n");
    printf("follow a corrupted next pointer, leading to SEGV.\n");

    // Now let's test this scenario
    printf("\n=== Reproduction test ===\n");

    // Manually create a scenario:
    // 1. Insert keys that map to a specific bucket
    // 2. Trigger kickout_bucket, which writes hash bits to _index[].slot
    // 3. The hash bits combined with slot can overflow the int boundary

    emhash8::HashMap<int, int> m;
    m.reserve(1);  // Force tiny table

    printf("After reserve(1): num_buckets=?, mask=?\n");

    // Insert some keys
    for (int i = 0; i < 10; i++) {
        m.insert({i * 1000, i});
    }

    printf("After inserts: size=%zu\n", m.size());
    for (int i = 0; i < 10; i++) {
        auto it = m.find(i * 1000);
        if (it == m.end()) {
            printf("BUG: key %d not found!\n", i * 1000);
        }
    }

    printf("Test completed without crash\n");
    return 0;
}
