# emhash
a very fast and efficient open address c++ flat hash map, you can bench it and compare the result with third party hash map.

some new feature:
1. the default load factor is 0.8, also be set to 1.0 by open compile marco set(average 5% performance loss)

2. head only with c++11/14/17, interface is highly compatible with std::unordered_map

3. it's the fastest hash map for find performance(100% hit), and good inserting if no rehash(call reserve before insert)

4. more memory efficient if the key.vlaue size is not aligned (size(key) % 8 != size(value) % 8) than other's implemention
for exmaple hash_map<long, int> can save 1/3 memoery the  hash_map<long, long>.

5. it's only one array allocted, a simple and smart collision algorithm.

6. it's can use a second hash if the input hash is bad with high collision.

7. lru is also used in main bucket if compile marco set

8. can dump hash collsion statics

9. no tombstones is used in my hash map. performance will not lost if high inserting and erasion.

10.more than 5 different flat hash map implemention to choose, each of them is some tiny different can be used in many case.
