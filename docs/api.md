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
| `try_get(key)` | Find element, returns pointer to value (`nullptr` on failure). Available in all versions (emhash5/6/7/8, emilib1/2/3/4) |
| `contains(key)` | Check if key exists |
| `count(key)` | Key occurrence count (0 or 1) |

## Modification Operations

| Method | Description |
|--------|-------------|
| `emplace(key, val)` | In-place construct insert |
| `try_emplace(key, args...)` | In-place construct only if key is absent; accepts zero or multiple mapped-value ctor args, no value construction when the key exists (emhash5/6/7/8, issue #104) |
| `insert({key, val})` | Insert key-value pair |
| `insert_unique(key, val)` | Direct insert (no existence check, best performance) |
| `try_set(key, val)` | Set value if key exists, do nothing if it doesn't. Returns `bool`. (emhash5/8, emilib1/2/3) |
| `set_get(key, val)` | Set new value, return old value (or default-constructed if key not found). Returns `ValueT`. (emhash5/8) |
| `set_get(key, val, oldv)` | Set new value, write old value to `oldv`. Returns `true` if key existed. (emilib1/2/3) |
| `erase(key)` / `erase(it)` | Delete element |
| `_erase(it)` | Delete element by iterator, return void (emhash5/6/7 and emilib1/2/3, faster than `erase()`) |
| `insert_or_assign(key, val)` | Insert or update value (emhash5/6/7/8, emilib1/2/3/4) |
| `get_or_return_default(key)` | Get value or default-constructed (emhash5/6/7/8 only) |
| `erase_if(pred)` | Erase elements matching predicate (emhash5/6/7/8, emilib1/2/3/4) |
| `equal_range(key)` | Get range of elements matching key (emhash5/6/7/8 only) |
| `merge(rhs)` | Merge another hash map (emhash5/6/7/8 only) |
| `clear()` | Clear all elements |

## Heterogeneous String Lookup (emhash8)

When both the hash and the equality types carry an `is_transparent` typedef
(e.g. `emhash8::string_hash` + `std::equal_to<>`), the following overloads
accept `std::string_view` and `const char*` directly for `std::string` keys —
no temporary `std::string` is constructed:

```cpp
emhash8::HashMap<std::string, int, emhash8::string_hash> map;
map.find(sv);        map.contains(sv);   map.count(sv);
map.at(sv);          map.erase(sv);      map.try_get(sv);
```

All forms content-hash identically to the equivalent `std::string` key.

## Other Functions

| Method | Description |
|--------|-------------|
| `hash(key)` | Public per-key hash (emhash8 only; the `EMH_HASH_LOOKUP` feature marker documents the precomputed-hash lookup overloads — `find/contains/count/at/try_get/erase` taking `hash(key)` — which are always available in emhash8) |
| `swap(a, b)` | ADL free-function swap (`swap(a.map, b.map)`) for emhash8::HashMap; all other containers provide the member `swap()` only |

## Iterators

```cpp
for (auto it = map.begin(); it != map.end(); ++it) {
    // it->first: key, it->second: value
}

// C++17 structured binding
for (const auto& [key, value] : map) { }
```
