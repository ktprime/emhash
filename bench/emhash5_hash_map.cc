#include <inttypes.h>
#include <string>
#include "hash_table5.hpp"

typedef emhash5::HashMap<int64_t, int64_t, std::hash<int64_t>> hash_t;
typedef emhash5::HashMap<std::string, int64_t, std::hash<std::string>> str_hash_t;

