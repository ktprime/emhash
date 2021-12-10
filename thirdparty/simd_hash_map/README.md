# simd_hash_map

## REQUIREMENTS TO BUILD: ##
* c++17
* Intel Processor with SSE2/SSE3, AVX2, or AVX512 instruction support
* `#include "path-to/simd_hash_map.hpp"`
* Example Build:
  * clang++ my_file.cpp -std=c++17 -mavx2
* See the following for optional x86 compiler flags
  * <https://gcc.gnu.org/onlinedocs/gcc/Option-Summary.html>
  * <https://clang.llvm.org/docs/ClangCommandLineReference.html#x86>

## API as of 26 April 2019 ##
* Template Parameters
  * `class Key`
  * `class T`
  * `class Hash = std::hash<Key>`
  * `class KeyEqual = std::equal_to<Key>`
* Member Types
  * `key_type`        : `Key`
  * `mapped_type`     : `T`
  * `value_type`      : `std::pair<const Key, T>`
  * `size_type`       : `std::size_t`
  * `difference_type` : `std::ptrdiff_t`
  * `hasher`          : `Hash`
  * `key_equal`       : `KeyEqual`
  * `reference`       : `value_type&`
  * `const_reference` : `const value_type&`
  * `pointer`         : `value_type*`
  * `const_pointer`   : `const value_type*`
  * `iterator`        : _LegacyForwardIterator_
  * `const_iterator`  : Constant _LegacyForwardIterator_
* Member Functions
  * `(constructor)`
  * `(destructor)`
* Iterators
  * `begin()` and `cbegin()`
    * Returns an iterator to the beginning.
  * `end()` and `cend()`
    * Returns an iterator to the end.
* Modifiers
  * `template<typename Args...>`
  * `try_emplace(key_type, Args &&...)`
    * return : `std::pair<iterator, bool>`
      * Iterator to key/value pair.
      * _True_: Emplaced with given Args.
      * _False_: Key already found in map, nothing occured.
    * Try to emplace key/value into the map.
    * If key is already present, nothing occurs
  * `template<typename Args...>`
  * `emplace_or_assign(key_type, Args &&...)`
    * return : `std::pair<iterator, bool>`
      * Iterator to key/value pair.
      * _True_: Emplaced with given key and Args.
      * _False_: Key already found in map, new value assigned.
    * Emplace key/value into the map.
  * `erase(key_type)`
    * return : `bool`
      * _True_: Key found, value_type erased
      * _False_: Key not found, nothing occured
    * Erase key/value with given key.
* Lookup
  * `find(key_type)`
    * return : `std::optional<iterator>`
      * Optional contains iterator if key was successfully found.
      * Iterator to key/value pair.
    * find key/value pair with given key
  * `contains(key_type)`
    * return : `bool`
      * _True_: Map conatins key.
      * _False_: Map does not contain key.
    * Check if map conatins key/value pair with given key.
  * `clear()`
    * return : `void`
    * Clear and reset the map.
    * Does not change max_size() of map.
* Capcaity
  * `size()`
    * return : `std::size_t`
    * Returns the size of the map (aka number of inserted pairs).
  * `max_size()`
    * return : `std::size_t`
    * Returns the maximum capacity of map before needing to grow/rehash.
  * `empty()`
    * return : `bool`
    * Checks if the map is empty or not.
* Hash Policy
  * `load_factor()`
    * return : `float`
    * Returns the load factor of the current map.
    * `load_factor() == size()/max_size()`
* Observers
  * `key_eq()`
    * return : `const KeyEqual&`
  * `hash_functions()`
    * return : `const Hash&`

__Major Differences between simd_hash_map and std::unordered_map__
* To optimize value semantics and reduce unnecessary moves and copies, the two "inserting" functions take a key and forwarded value arguments separately. If value_type is emplaced, it is constructed as `value_type {std::piecewise_construct, std::forward_as_tuple(key_type), std::forward_as_tuple(std::forward<Args>(args)...)};`. If just the mapped_type is inserted due to the key already existing, the following occurs: `old_mapped_value = std::move(mapped_type{std::forward<Args>(args)...});`.
* Due to the nature of how this hash table deals with collisions, the user cannot determine the load factor as when the hash table will grow in size. 
* `find(key_type)` returns an `std::optional<iterator>`. This is because I try to practice declarative programming, and returning an `std::optional` is IMO more delcarative than just an iterator which could point to the end of the map.
* As of 26 April 2019, this container does not take custom allocators, however, this feature will be implemented in the future.


## Tests ##
To run tests, compile as follow inside the tests/ directory

gcc tests-main.cpp "test file"
