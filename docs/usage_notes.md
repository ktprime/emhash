# Usage Notes

## 0. Thread Safety

emhash is **not thread-safe**. Concurrent access from multiple threads requires external synchronization:

```cpp
// ❌ Wrong: concurrent access without synchronization
// Thread 1: map[key] = val;
// Thread 2: map.find(key);

// ✅ Correct: use mutex for concurrent access
std::mutex mtx;
{
    std::lock_guard<std::mutex> lock(mtx);
    map[key] = val;
}

// ✅ Correct: concurrent read-only access is safe
// Multiple threads can call find()/contains()/count() simultaneously
// as long as no thread is modifying the map
```

## 1. Non-Node-Based Hash Table

emhash **does not guarantee** reference stability during insert/erase/rehash operations.

```cpp
// ❌ Wrong: reference may become invalid after rehash
auto& ref = myhash[1];
myhash[2] = ref;  // Danger! May trigger rehash

// ✅ Correct: copy value first
auto val = myhash[1];
myhash.reserve(10000);  // Pre-allocate
myhash[2] = val;
```

## 2. Erasing During Iteration

Deleting elements during iteration may cause some keys to be visited twice or skipped.

```cpp
// ❌ Wrong: direct deletion without updating iterator
for (const auto& it : myhash) {
    if (condition) myhash.erase(it.first);
}

// ✅ Correct: use iterator erase and update
for (auto it = myhash.begin(); it != myhash.end(); ) {
    if (condition) {
        it = myhash.erase(it);  // Returns next valid iterator
    } else {
        ++it;
    }
}
```

## 3. Large Value Type Optimization

For very large value types (e.g., `> 100 bytes`), consider using pointer storage:

```cpp
// High memory usage
emhash7::HashMap<Key, LargeValue> map1;

// Optimized memory usage
emhash7::HashMap<Key, std::unique_ptr<LargeValue>> map2;
```
