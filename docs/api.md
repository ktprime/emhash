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
| `try_set(key, val)` | Set value if key exists, do nothing if it doesn't (emhash5/8) |
| `set_get(key, val)` | Set new value, return old value (or default if key not found) (emhash5/8) |
| `erase(key)` / `erase(it)` | Delete element |
| `_erase(it)` | Delete element by iterator, return void (emhash5/6/7, faster) |
| `insert_or_assign(key, val)` | Insert or update value (all versions) |
| `get_or_return_default(key)` | Get value or default-constructed (all versions) |
| `erase_if(pred)` | Erase elements matching predicate (all versions) |
| `equal_range(key)` | Get range of elements matching key (all versions) |
| `merge(rhs)` | Merge another hash map (all versions) |
| `clear()` | Clear all elements |

## Iterators

```cpp
for (auto it = map.begin(); it != map.end(); ++it) {
    // it->first: key, it->second: value
}

// C++17 structured binding
for (const auto& [key, value] : map) { }
```
