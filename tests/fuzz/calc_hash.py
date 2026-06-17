h1 = 4294930287  # std::hash<int>()(-37009) as uint32_t
h2 = 1862336511
MAP_BITS = 253
EFILLED = -126

h2_tag1 = (h1 % MAP_BITS) + EFILLED
h2_tag2 = (h2 % MAP_BITS) + EFILLED
print(f"h2 tag for -37009: {h2_tag1}")
print(f"h2 tag for 1862336511: {h2_tag2}")
print(f"main_bucket for -37009: {h1 & 15}")
print(f"main_bucket for 1862336511: {h2 & 15}")
