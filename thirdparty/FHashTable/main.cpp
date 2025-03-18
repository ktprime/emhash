#include "fhash_table.h"
#include <map>
#include <stdlib.h>
#include <unordered_map>
#include <iostream>
#include <algorithm>
#include <unordered_set>
#include <chrono>
#include <cmath>

template <bool remove_duplicated>
std::vector<int64_t> gen_random_data(int32_t N)
{
	std::vector<int64_t> data;
	data.reserve(N);
	std::unordered_set<int64_t> numbers;
	for (int32_t i = 0; i < N; i++)
	{
		int64_t r = ((int64_t)rand() << 32) | rand();
		if (remove_duplicated)
		{
			if (numbers.insert(r).second)
			{
				data.push_back(r);
			}
		}
		else
		{
			data.push_back(r);
		}
	}
	return data;
}

void functional_test()
{
	{
		fhash_table<int32_t, int32_t> h;
		assert(h.find(0) == nullptr);
		assert(!h.erase(0));
		h.validate();
		uint32_t Sum = 0;
		for (auto&& pr : h)
		{
			Sum += pr.second;
		}
		assert(Sum == 0);
	}
	{
		fhash_table<int32_t, int32_t> h;
		h.insert(1, 1);
		h.validate();
		assert(h.find(0) == nullptr);
		assert(h.find(1) != nullptr);
		uint32_t Sum = 0;
		for (auto&& pr : h)
		{
			Sum += pr.second;
		}
		assert(Sum == 1);
		assert(h.erase(0) == h.end());

		auto end_it = h.end();
		auto erased_it = h.erase(1);
		assert(erased_it == end_it);
		h.validate();
	}
	{
		fhash_table<int32_t, int32_t> h;
		for (int32_t i = 0; i < 10; i++)
		{
			h.insert(i, i);
			assert(h.find(i) != nullptr);
			h.erase(i);
			assert(h.find(i) == nullptr);
			h.validate();
		}
	}
	{
		using fhash_table_t = fhash_table<int64_t, int64_t>;
		fhash_table_t h;
		std::vector<int64_t> data = gen_random_data<true>(1000);
		for (size_t i = 0; i < data.size(); i++)
		{
			int64_t d = data[i];
			h.insert(d, int64_t(i));
			assert(h.find(d) != nullptr);
			h.validate();
		}
		std::vector<typename fhash_table_t::fhash_size_t> distances = h.get_distance_stats();
		int64_t sum = 0;
		for (size_t i = 0; i < distances.size(); i++)
		{
			sum += distances[i] * i;
		}
		int64_t sum10 = 0;
		for (size_t i = 0; i < 10; i++)
		{
			sum10 += distances[i];
		}
		float avg = float(sum) / h.size();
		double load_factor = h.load_factor();
		float factor10 = float(sum10) / h.size();
		for (size_t i = 0; i < data.size(); i++)
		{
			int64_t d = data[i];
			const int64_t* pi = h.find(d);
			assert(*pi == i);
		}

		std::random_shuffle(data.begin(), data.end());
		for (size_t i = 0; i < data.size(); i++)
		{
			h.erase(data[i]);
			assert(h.find(data[i]) == nullptr);
			h.validate();
		}
	}

	// random test.
	{
		using fhash_table_t = fhash_table<int32_t, int32_t>;
		fhash_table_t h;
		for (uint32_t j = 0; j < 100; j++)
		{
			const uint32_t N = 1000;
			for (uint32_t i = 0; i < N; i++)
			{
				h.insert(rand(), i);
			}

			uint32_t deleted = 0;
			typename fhash_table_t::fhash_size_t total = h.size();
			for (uint32_t k = 0; k < 10; k++)
			{
				for (auto it = h.begin(); it < h.end();)
				{
					if (rand() & 1)
					{
						it = h.erase(it);
						deleted++;
					}
					else
					{
						it++;
					}
				}
			}
			assert(total == h.size() + deleted);
		}
	}
}

static void test_find_success()
{
	for (int32_t i = 1; i < 15; i++)
	{
		const int32_t N = int32_t(std::pow(3, i));
		std::cout << "N = " << N << std::endl;
		std::vector<int64_t> data = gen_random_data<true>(N);
		{
			fhash_table<int64_t, int64_t> m;
			for (int64_t i : data)
			{
				m.insert(i, i);
			}
			std::vector<int64_t> shuffled_data = data;
			std::random_shuffle(shuffled_data.begin(), shuffled_data.end());
			auto start = std::chrono::high_resolution_clock::now();
			int64_t sum = 0;
			for (int32_t i = 0; i < 100000000 / N; i++)
			{
				for (int64_t i : shuffled_data)
				{
					sum += *m.find(i);
				}
			}
			auto end = std::chrono::high_resolution_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
			std::cout << "fhash_table, elapsed milliseconds: " << elapsed << " sum: " << sum << " load_factor: " << m.load_factor() << std::endl;
		}
		{
			std::unordered_map<int64_t, int64_t> m;
			for (int64_t i : data)
			{
				m.emplace(i, i);
			}
			std::vector<int64_t> shuffled_data = data;
			std::random_shuffle(shuffled_data.begin(), shuffled_data.end());
			int64_t sum = 0;
			auto start = std::chrono::high_resolution_clock::now();
			for (int32_t i = 0; i < 100000000 / N; i++)
			{
				for (int64_t i : shuffled_data)
				{
					sum += m.find(i)->second;
				}
			}
			auto end = std::chrono::high_resolution_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
			std::cout << "std::unordered_map, elapsed milliseconds: " << elapsed << " sum: " << sum << std::endl;
		}
	}
}

static void perf_test()
{
	test_find_success();
}

int main()
{
	functional_test();
	perf_test();
	return 0;
}
