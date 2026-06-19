# API Reference

## Constructors

```cpp
HashMap();                                    // Default constructor
HashMap(size_t bucket_count);                 // Specify initial bucket count
HashMap(size_t bucket_count, float max_load_factor);  // Specify load factor
HashMap(const HashMap& other);                // Copy constructor
HashMap(HashMap&& other) noexcept;            // Move constructor
HashMap(std::initializer_list<value_type> il); // Initializer list
```

## Capacity Operations

| Method | Description |
|--------|-------------|
| `size()` / `empty()` | Element count / is empty |
| `bucket_count()` | Current bucket count |
| `load_factor()` / `max_load_factor()` | Current/max load factor |
| `reserve(n)` | Reserve space for at least n elements |
| `shrink_to_fit()` | Shrink to fit current size |

## Element Access

| Method | Description |
|--------|-------------|
| `operator[key]` | Insert or access element |
| `at(key)` | Access element, throws `std::out_of_range` if not found |
| `find(key)` | Find element, returns iterator |
| `try_get(key)` | Find element, returns pointer to value (`nullptr` on failure) |
| `contains(key)` | Check if key exists |
| `count(key)` | Key occurrence count (0 or 1) |

## Modification Operations

| Method | Description |
|--------|-------------|
| `emplace(key, val)` | In-place construct insert |
| `insert({key, val})` | Insert key-value pair |
| `insert_unique(key, val)` | Direct insert (no existence check, best performance) |
| `try_set(key, val)` | Set only if key does not exist (emhash5/8) |
| `set_get(key, val)` | Set new value, return old value (emhash5/8) |
| `erase(key)` / `erase(it)` | Delete element |
| `_erase(key)` | Delete element, return void (faster) |
| `clear()` | Clear all elements |

## Iterators

```cpp
for (auto it = map.begin(); it != map.end(); ++it) {
    // it->first: key, it->second: value
}

// C++17 structured binding
for (const auto& [key, value] : map) { }
```
