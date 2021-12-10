#include <inttypes.h>
#include <string>
#include "hash_table6.hpp"

typedef emhash6::HashMap<int64_t, int64_t, std::hash<int64_t>> hash_t;
typedef emhash6::HashMap<std::string, int64_t, std::hash<std::string>> str_hash_t;

