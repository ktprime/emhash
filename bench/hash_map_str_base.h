#define SETUP_STR \
	str_hash_t str_hash;
#define RESERVE_STR(size) \
	str_hash.reserve(size);
#define LOAD_FACTOR_STR_HASH(str_hash) \
	str_hash.load_factor()
#define INSERT_STR(key, value) \
	str_hash.insert(str_hash_t::value_type(key, value));
#define DELETE_STR(key) \
	str_hash.erase(key);
#define FIND_STR_EXISTING(key) \
	if(str_hash.find(key) == str_hash.end()) { \
	}
#define FIND_STR_MISSING(key) \
	if(str_hash.find(key) != str_hash.end()) { \
	}
#define FIND_STR_EXISTING_COUNT(key, count) \
	if(str_hash.find(key) != str_hash.end()) { \
		count++; \
	}
#define CLEAR_STR
