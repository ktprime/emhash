# emhash
a very fast and efficient c++ flat hash map, you can bench and compare the result with third party hash map.

some feature
#1. the default load factor is 0.8, also be reache to 1.0 by open compile marco
#2. head only with c++11, more than 5 different flat hash map implemention to choose
#3. for small k.v, it's the fastest hash map for find performance.
#4. more memory efficient if the key.vlaue is aligned (size(key) % 8 != size(value) % 8) than other's implemention,
for exmaple hash_map<long, int> can save 1/3 memoery the  hash_map<long, long>.
#5. it's only one array allocted, a simple and smart collision algorithm.
#6. it's can use a second hash if the input hash is bad with high collision.
#7. lru is also used in main bucket if compile marco set
#8. can dump hash collsion statics
