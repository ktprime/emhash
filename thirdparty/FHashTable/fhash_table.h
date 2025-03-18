#pragma once
#include <stdint.h>
#include <assert.h>
#include <memory>
#include <vector>
#include <limits>
#include <cmath>

struct fhash_default_allocator_policy
{
	static constexpr int32_t average_number_of_elements_per_bucket100 = 150;
	static constexpr int32_t min_number_of_hash_buckets = 2;
	static constexpr int32_t min_number_of_entries = 4;
	using fhash_size_t = int32_t;
	// consider using int64_t if the table size may be larger than 2^30, with the cost of performance.
	// using fhash_size_t = int64_t;
};

template <typename key_t, typename value_t, typename hasher_t = std::hash<key_t>, typename allocator_policy = fhash_default_allocator_policy>
class fhash_table
{
public:
	using fhash_size_t = typename allocator_policy::fhash_size_t;
	template <typename raw_integer_t, typename tag>
	struct integer_t
	{
		raw_integer_t value;
		integer_t() = default;
		constexpr explicit integer_t(raw_integer_t v) :value(v) {}
		integer_t operator = (integer_t other){ value = other.value; return *this; }

		bool operator < (integer_t other) const {return value < other.value;}
		bool operator <= (integer_t other) const { return value <= other.value; }
		bool operator == (integer_t other) const { return value == other.value; }
		bool operator != (integer_t other) const { return value != other.value; }
		bool operator >= (integer_t other) const { return value >= other.value; }
		bool operator > (integer_t other) const { return value > other.value; }
		integer_t operator--(int) { integer_t tmp = *this; value--; return tmp; }
		integer_t operator++(int) { integer_t tmp = *this; value++; return tmp; }

		friend integer_t operator + (integer_t a, integer_t b) { return integer_t(a.value + b.value); }
		friend integer_t operator - (integer_t a, integer_t b) { return integer_t(a.value - b.value); }
		friend integer_t operator * (integer_t a, integer_t b) { return integer_t(a.value * b.value); }
		friend integer_t operator / (integer_t a, integer_t b) { return integer_t(a.value / b.value); }
	};

	struct tag_index {};
	struct tag_node_index {};
	struct tag_hash {};

	using index_t = integer_t<fhash_size_t, tag_index>;
	using node_index_t = integer_t<fhash_size_t, tag_node_index>;
	using hash_t = integer_t<fhash_size_t, tag_hash>;

	static constexpr index_t invalid_index = index_t(-1);
	static constexpr node_index_t invalid_node_index = node_index_t(-2);

	static_assert(allocator_policy::min_number_of_entries >= allocator_policy::min_number_of_hash_buckets, 
		"allocator_policy::min_number_of_entries >= allocator_policy::min_number_of_hash_buckets");

	static_assert(allocator_policy::min_number_of_hash_buckets > 0,
		"allocator_policy::min_number_of_hash_buckets > 0");

	// node index start from -3 to -inf.
	// index + node_index + 3 = 0.
	static index_t node_index_to_index(node_index_t node_index)
	{
		return index_t(-3 - node_index.value);
	}

	static index_t node_index_to_index_check_invalid(node_index_t node_index)
	{
		if (node_index == invalid_node_index)
		{
			return invalid_index;
		}
		else
		{
			return node_index_to_index(node_index);
		}
	}

	static node_index_t index_to_node_index(index_t index)
	{
		return node_index_t(-3 - index.value);
	}

	struct data
	{
		data() {}
		data(key_t, value_t) {}
		index_t prev;
		index_t next;
		std::aligned_storage_t<sizeof(key_t), alignof(key_t)> key;
		std::aligned_storage_t<sizeof(value_t), alignof(value_t)> value;

		void construct(key_t k, value_t v)
		{
			new (&key) key_t(k);
			new (&value) value_t(v);
		}

		void destruct()
		{
			get_key().~key_t();
			get_value().~value_t();
		}

		key_t& get_key()
		{
			return reinterpret_cast<key_t&>(key);
		}

		value_t& get_value()
		{
			return reinterpret_cast<value_t&>(value);
		}

		const key_t& get_key() const
		{
			return reinterpret_cast<const key_t&>(key);
		}

		const value_t& get_value() const
		{
			return reinterpret_cast<const value_t&>(value);
		}
	};

	struct node
	{
		node_index_t& get_child_index(index_t dir)
		{
			node_index_t* children = &lchild;
			return children[dir.value];
		}

		node_index_t lchild;
		node_index_t rchild;
		node_index_t parent;
	};

	union entry
	{
		data d;
		node n;
		bool is_data() const
		{
			return d.prev >= invalid_index;
		}
	};

	template <bool bConst>
	struct base_iterator
	{
		typedef typename std::conditional<bConst, const fhash_table, fhash_table>::type table_t;
		typedef typename std::conditional<bConst, const key_t, key_t>::type it_key_t;
		typedef typename std::conditional<bConst, const value_t, value_t>::type it_value_t;

		base_iterator(table_t& table, index_t index)
			: m_table(&table)
			, m_index(index)
		{
			skip_empty();
		}

		friend bool operator==(const base_iterator& lhs, const base_iterator& rhs)
		{
			return lhs.m_index == rhs.m_index;
		}

		friend bool operator!=(const base_iterator& lhs, const base_iterator& rhs)
		{
			return !(lhs == rhs);
		}

		friend bool operator<(const base_iterator& lhs, const base_iterator& rhs)
		{
			return lhs.m_index < rhs.m_index;
		}

		base_iterator& operator++()
		{
			m_index++;
			skip_empty();
			return *this;
		}

		base_iterator operator++(int)
		{
			base_iterator copy(*this);
			++* this;
			return copy;
		}

		void skip_empty()
		{
			for (; *this && !m_table->get_entry(m_index).is_data(); m_index++);
		}

		it_key_t& key() const { return m_table->get_entry(m_index).d.get_key(); }

		it_value_t& value() const { return m_table->get_entry(m_index).d.get_value(); }

		std::pair<const key_t&, it_value_t&> operator* () const { return std::pair<const key_t&, it_value_t&>(key(), value()); }

		/** conversion to "bool" returning true if the iterator is valid. */
		explicit operator bool() const
		{
			return m_index < index_t(m_table->capacity());
		}

		/** inverse of the "bool" operator */
		bool operator !() const
		{
			return !(bool)*this;
		}

	private:
		friend class fhash_table;
		table_t* m_table = nullptr;
		index_t m_index = index_t(0);
	};

	template <bool>
	friend struct base_iterator;

	class iterator : public base_iterator<false>
	{
		using super = base_iterator<false>;
	public:
		using super::super;
		iterator& operator=(const iterator&) = default;
	};

	class const_iterator : public base_iterator<true>
	{
		using super = base_iterator<true>;
	public:
		using super::super;
		const_iterator& operator=(const const_iterator&) = default;
	};

	iterator      begin() { return make_iterator(0); }

	const_iterator begin() const { return make_const_iterator(0); }

	// be caustion, erase will change the end iterator.
	iterator      end() { return make_iterator(capacity()); }

	const_iterator end() const { return make_const_iterator(capacity()); }

	fhash_table() = default;

	fhash_table(fhash_table&& other)
	{
		move_table(std::move(other));
	}

	fhash_table(const fhash_table& other)
	{
		copy_table(other);
	}

	fhash_table& operator = (fhash_table&& other)
	{
		clear();
		move_table(std::move(other));
		return *this;
	}

	fhash_table& operator = (const fhash_table& other)
	{
		clear();
		copy_table(other);
		return *this;
	}

	~fhash_table()
	{
		clear();
	}

	void move_table(fhash_table&& other)
	{
		m_entries = other.m_entries;
		other.m_entries = get_default_entries();

		m_entries_size = other.m_entries_size;
		other.m_entries_size = allocator_policy::min_number_of_entries;

		m_bucket_size_minus_one = other.m_bucket_size_minus_one;
		other.m_bucket_size_minus_one = allocator_policy::min_number_of_hash_buckets - 1;

		m_size = other.m_size;
		other.m_size = 0;

		m_root = other.m_root;
		other.m_root = invalid_index;

		m_max_index = other.m_max_index;
		other.m_max_index = invalid_index;
	}

	void copy_table(const fhash_table& other)
	{
		reserve(other.size());

		// now insert old data to new table.
		if (other.m_entries != get_default_entries())
		{
			const fhash_size_t cap = other.capacity();
			for (fhash_size_t i = 0; i < cap; i++)
			{
				const entry& e = other.get_entry(index_t(i));
				if (e.is_data() && e.d.prev == invalid_index)
				{
					insert_index_no_check(compute_hash_slot(e.d.get_key()), e.d.get_key(), e.d.get_value());
				}
			}

			for (fhash_size_t i = 0; i < cap; i++)
			{
				const entry& e = other.get_entry(index_t(i));
				if (e.is_data() && e.d.prev != invalid_index)
				{
					insert_index_no_check(compute_hash_slot(e.d.get_key()), e.d.get_key(), e.d.get_value());
				}
			}
		}
	}

	void clear()
	{
		if (m_entries != get_default_entries())
		{
			for (fhash_size_t i = 0; i < m_entries_size; i++)
			{
				entry& e = m_entries[i];
				if (e.is_data())
				{
					e.d.destruct();
				}
			}
			free(m_entries);
			m_entries = get_default_entries();
		}
		m_entries_size = allocator_policy::min_number_of_entries;
		m_bucket_size_minus_one = allocator_policy::min_number_of_hash_buckets - 1;
		m_size = 0;
		m_root = invalid_index;
		m_max_index = invalid_index;
	}

	const value_t* find(key_t key) const
	{
		return find_index(key, compute_hash_slot(key),
			[this](index_t index) {return &get_entry(index).d.get_value(); },
			[this]() {return (const value_t*)nullptr; }
			);
	}

	value_t* find(key_t key)
	{
		return find_index(key, compute_hash_slot(key),
			[this](index_t index) {return &get_entry(index).d.get_value(); },
			[]() {return (value_t*)nullptr; }
		);
	}

	iterator insert(key_t key, value_t value)
	{
		const hash_t hash = compute_hash(key);
		const index_t index = find_index(key, compute_slot(hash), 
			[](index_t index) {return index; },
			[]() {return invalid_index; });
		if (index != invalid_index)
		{
			// alread exists, replace it.
			get_entry(index).d.get_value() = value;
			return make_iterator(index);
		}
	
		reserve(m_size + 1);
		return make_iterator(insert_index_no_check(compute_slot(hash), key, value));
	}

	iterator emplace(key_t key, value_t value)
	{
		const hash_t hash = compute_hash(key);
		const index_t index = find_index(key, compute_slot(hash),
			[](index_t index) {return index; },
			[]() {return invalid_index; });
		if (index != invalid_index)
		{
			// alread exists, replace it.
			get_entry(index).d.get_value() = value;
			return make_iterator(index);
		}

		reserve(m_size + 1);
		return make_iterator(insert_index_no_check(compute_slot(hash), key, value));
	}

	iterator erase(key_t key)
	{
		return find_index(key, compute_hash_slot(key),
			[this](index_t index) {return erase(make_iterator(index)); },
			[this]() {return make_iterator(capacity()); });
	}

	void validate() const
	{
		if (m_entries != get_default_entries())
		{
			std::vector<bool> visited;
			visited.resize(m_entries_size);
			fhash_size_t size = 0;
			fhash_size_t visited_size = 0;
			for (fhash_size_t i = 0; i < m_entries_size; i++)
			{
				index_t index = index_t(i);
				const entry* e = &get_entry(index);
				if (e->is_data())
				{
					if (e->d.prev == invalid_index)
					{
						for (; index != invalid_index; index = e->d.next, e = &get_entry(index))
						{
							assert(!visited[index.value]);
							if (e->d.prev != invalid_index)
							{
								assert(get_entry(e->d.prev).d.next == index);
							}
							if (e->d.next != invalid_index)
							{
								assert(get_entry(e->d.next).d.prev == index);
							}
							visited[index.value] = true;
							visited_size++;
						}
					}
					size++;
				}
			}

			assert(size == visited_size);
			assert(size == m_size);
			
			fhash_size_t tree_size = validate_tree(m_root);
			assert(tree_size + m_size == m_entries_size);
		}
	}

	void reserve(fhash_size_t expected_size)
	{
		if (allocatable_bucket_size() < get_number_of_hash_buckets(expected_size) || expected_size > m_entries_size)
		{
			rehash(expected_size);
		}
	}

	std::vector<fhash_size_t> get_distance_stats() const
	{
		std::vector<fhash_size_t> distances;
		if (m_entries == get_default_entries())
		{
			return distances;
		}
		for (fhash_size_t i = 0; i < m_entries_size; i++)
		{
			index_t index = index_t(i);
			const entry* e = &get_entry(index);
			if (e->is_data() && e->d.prev == invalid_index)
			{	
				index_t prev_index = index;
				for (; index != invalid_index; index = e->d.next, e = &get_entry(index))
				{
					const std::make_unsigned_t<fhash_size_t> distance = std::abs((index - prev_index).value);
					prev_index = index;
					if (distances.size() <= distance)
					{
						distances.resize(distance + 1);
					}
					distances[distance]++;
				}
			}
		}
		return distances;
	}

	fhash_size_t size() const
	{
		return m_size;
	}

	double load_factor() const
	{
		if (allocatable_bucket_size() > 0)
		{
			return double(m_size) / allocatable_bucket_size();
		}
		else
		{
			return 0.0f;
		}
	}

	float max_load_factor(float lf) const
	{
		if (allocatable_bucket_size() > 0)
		{
			return double(m_size) / allocatable_bucket_size();
		}
		else
		{
			return 0.0f;
		}
	}

	fhash_size_t capacity() const
	{
		return m_max_index.value + 1;
	}

	iterator erase(iterator it)
	{
		if (it < end())
		{
			return make_iterator(remove_index(it.m_index));
		}
		else
		{
			return end();
		}
	}

private:

	iterator make_iterator(fhash_size_t index)
	{
		return iterator(*this, index_t(index));
	}

	const_iterator make_const_iterator(fhash_size_t index)
	{
		return const_iterator(*this, index_t(index));
	}

	iterator make_iterator(index_t index)
	{
		return iterator(*this, index);
	}

	const_iterator make_const_iterator(index_t index)
	{
		return const_iterator(*this, index);
	}

	fhash_size_t validate_tree(index_t index) const
	{
		if (index == invalid_index)
		{
			return 0;
		}
		fhash_size_t size = 1;
		const node& n = get_node(index);
		if (n.lchild != invalid_node_index)
		{
			assert(get_node(n.lchild).parent == index_to_node_index(index));
			size += validate_tree(node_index_to_index(n.lchild));
		}
		if (n.rchild != invalid_node_index)
		{
			assert(get_node(n.rchild).parent == index_to_node_index(index));
			size += validate_tree(node_index_to_index(n.rchild));
		}
		return size;
	}

	hash_t compute_hash(key_t key) const
	{
		return hash_t(m_hasher(key));
	}

	index_t compute_slot(hash_t h) const
	{
		return index_t(h.value & m_bucket_size_minus_one);
	}

	index_t compute_hash_slot(key_t key)
	{
		return compute_slot(compute_hash(key));
	}

	void insert_empty(data& d, key_t key, value_t value)
	{
		d.construct(key, value);
		d.next = invalid_index;
		d.prev = invalid_index;
	}

	index_t allocate_entry(index_t index)
	{
		index_t pos = find_min_distance_node(index);
		assert(pos != invalid_index);
		// remove from tree.
		remove_node(pos);
		return pos;
	}

	index_t insert_tail(index_t index, key_t key, value_t value)
	{
		index_t new_index = allocate_entry(index);
		index_t prev = index;
		while (index != invalid_index)
		{
			prev = index;
			index = get_entry(index).d.next;
		}
		data& p = get_entry(prev).d;
		data& t = get_entry(new_index).d;
		p.next = new_index;
		t.prev = prev;
		t.next = invalid_index;
		t.construct(key, value);
		update_max_index(new_index);
		return new_index;
	}

	index_t insert_index_no_check(index_t index, key_t key, value_t value)
	{
		entry& e = get_entry(index);
		data& d = e.d;
		if (e.is_data())
		{
			if (d.prev != invalid_index)
			{
				// we are list from other slot.
				key_t victim_key = std::move(d.get_key());
				value_t victim_value = std::move(d.get_value());

				d.destruct();

				const index_t unlinked_index = unlink_index(index);
				assert(unlinked_index == index);
				insert_empty(d, key, value);

				update_max_index(index);

				insert_index_no_check(compute_hash_slot(victim_key), victim_key, victim_value);
				return index;
			}
			else
			{
				m_size++;
				return insert_tail(index, key, value);
			}
		}
		else
		{
			m_size++;
			remove_node(index);
			insert_empty(d, key, value);
			update_max_index(index);
			return index;
		}
	}

	void update_max_index(index_t index)
	{
		if (m_max_index < index)
		{
			m_max_index = index;
		}
	}

	index_t unlink_index(index_t index)
	{
		entry& e = get_entry(index);
		data& d = e.d;
		assert(e.is_data());
		// fix previous.
		if (d.prev != invalid_index)
		{
			get_entry(d.prev).d.next = d.next;
			if (d.next != invalid_index)
			{
				get_entry(d.next).d.prev = d.prev;
			}
		}
		else
		{
			// i'm the first, move the next to first and unlink the next.
			const index_t next_index = d.next;
			if (next_index != invalid_index)
			{
				data& next = get_entry(next_index).d;
				const index_t unlinked_index = unlink_index(next_index);
				assert(unlinked_index == next_index);

				d.get_key() = std::move(next.get_key());
				d.get_value() = std::move(next.get_value());
				
				index = unlinked_index;
			}
		}
		return index;
	}

	index_t remove_index(index_t index)
	{
		const index_t unlinked_index = unlink_index(index);
		entry& e = get_entry(unlinked_index);
		assert(e.is_data());
		e.d.destruct();
		add_node(unlinked_index);
		m_size--;
		while (m_max_index > invalid_index && !get_entry(m_max_index).is_data()) m_max_index--;
		if (unlinked_index > index)
		{
			// unlinked an next from the future.
			return index;
		}
		else
		{
			return index + index_t(1);
		}
	}

	template <typename success_operation_t, typename failed_operation_t>
	decltype(auto) find_index(key_t key, index_t index, success_operation_t success_operation, failed_operation_t failed_operation) const
	{
		const entry* e = &get_entry(index);
		if (!e->is_data())
		{
			return failed_operation();
		}

		do
		{
			if (e->d.get_key() == key)
			{
				return success_operation(index);
			}
			index = e->d.next; 
			if (index == invalid_index)
			{
				return failed_operation();
			}
			e = &get_entry(index);
		} while(true);
	}

	template <typename integer_t>
	static integer_t next_power_of_2(integer_t v)
	{
		std::make_unsigned_t<integer_t> uv = v;

		// this loop only works for unsigned integer type.
		uv--;
		for (size_t i = 1; i < sizeof(uv) * 8; i *= 2)
		{
			uv |= uv >> i;
		}
		++uv;

		v = uv;
		assert(v >= 0);
		return v;
	}

	static fhash_size_t get_number_of_hash_buckets(fhash_size_t expected_size)
	{
		const fhash_size_t expected_bucket_num = expected_size * 100 / allocator_policy::average_number_of_elements_per_bucket100 + allocator_policy::min_number_of_hash_buckets;
		return next_power_of_2(expected_bucket_num);
	}

	void rehash(fhash_size_t expected_size)
	{
		fhash_table old_table(std::move(*this));

		const fhash_size_t bucket_size = get_number_of_hash_buckets(expected_size);
		m_bucket_size_minus_one = bucket_size - 1;

		m_entries_size = std::max(bucket_size * allocator_policy::average_number_of_elements_per_bucket100 / 100, expected_size);
		// rehash shoudn't throw any data.
		m_entries_size = std::max(m_entries_size, m_size);

		m_entries = (entry*)malloc(m_entries_size * sizeof(entry));

		// build the tree.
		m_root = build_tree(index_t(0), index_t(m_entries_size));
		get_node(m_root).parent = invalid_node_index;

		// now insert old data to new table.
		if (old_table.m_entries != get_default_entries())
		{
			const fhash_size_t cap = old_table.capacity();
			for (fhash_size_t i = 0; i < cap; i++)
			{
				entry& e = old_table.get_entry(index_t(i));
				if (e.is_data() && e.d.prev == invalid_index)
				{
					insert_index_no_check(compute_hash_slot(e.d.get_key()), e.d.get_key(), e.d.get_value());
				}
			}

			for (fhash_size_t i = 0; i < cap; i++)
			{
				entry& e = old_table.get_entry(index_t(i));
				if (e.is_data() && e.d.prev != invalid_index)
				{
					insert_index_no_check(compute_hash_slot(e.d.get_key()), e.d.get_key(), e.d.get_value());
				}
			}
		}
	}

private:

	entry& get_entry(index_t index)
	{
		return m_entries[index.value];
	}

	entry& get_entry(node_index_t node_index)
	{
		return get_entry(node_index_to_index(node_index));
	}

	const entry& get_entry(index_t index) const
	{
		return m_entries[index.value];
	}

	const entry& get_entry(node_index_t node_index) const
	{
		return get_entry(node_index_to_index(node_index));
	}

	node& get_node(index_t index)
	{
		return get_entry(index).n;
	}

	node& get_node(node_index_t node_index)
	{
		return get_entry(node_index).n;
	}

	const node& get_node(index_t index) const
	{
		return get_entry(index).n;
	}

	const node& get_node(node_index_t node_index) const
	{
		return get_entry(node_index).n;
	}

	// tree operation.
	index_t build_tree(index_t begin, index_t end)
	{
		if (begin == end)
		{
			return invalid_index;
		}
		const index_t mid = (begin + end) / index_t(2);
		node& root = get_node(mid);

		const index_t lchild = build_tree(begin, mid);
		const index_t rchild = build_tree(mid + index_t(1), end);

		if (lchild != invalid_index)
		{
			get_node(lchild).parent = index_to_node_index(mid);
			root.lchild = index_to_node_index(lchild);
		}
		else
		{
			root.lchild = invalid_node_index;
		}

		if (rchild != invalid_index)
		{
			get_node(rchild).parent = index_to_node_index(mid);
			root.rchild = index_to_node_index(rchild);
		}
		else
		{
			root.rchild = invalid_node_index;
		}

		return mid;
	}

	index_t get_node_dir(node_index_t index)
	{
		node& n = get_node(index);
		if (n.parent == invalid_node_index)
		{
			return invalid_index;
		}
		node& p = get_node(n.parent);
		if (p.lchild == index)
		{
			return index_t(0);
		}
		else if (p.rchild == index)
		{
			return index_t(1);
		}
		else
		{
			assert(false);
			return invalid_index;
		}
	}

	index_t step(index_t index, index_t dir)
	{
		node_index_t pchild = invalid_node_index;
		node_index_t child = get_node(index).get_child_index(dir);
		while (child != invalid_node_index)
		{
			pchild = child;
			child = get_node(child).get_child_index(index_t(1) - dir);
		}
		if (pchild == invalid_node_index)
		{
			return invalid_index;
		}
		else
		{
			return node_index_to_index(pchild);
		}
	}

	void remove_node(index_t erased_index)
	{
		const node_index_t erased_node_index = index_to_node_index(erased_index);

		// copied from std::_Tree_val::_Extract
		node_index_t erased_node = erased_node_index;
		node_index_t fixnode;
		node_index_t fixnode_parent;
		node_index_t pnode = erased_node;
		if (get_node(pnode).lchild == invalid_node_index)
		{
			fixnode = get_node(pnode).rchild;
		}
		else if (get_node(pnode).rchild == invalid_node_index)
		{
			fixnode = get_node(pnode).lchild;
		}
		else
		{
			pnode = index_to_node_index(step(erased_index, index_t(1)));
			fixnode = get_node(pnode).rchild;
		}

		if (pnode == erased_node)
		{
			fixnode_parent = get_node(erased_node).parent;
			if (fixnode != invalid_node_index)
			{
				get_node(fixnode).parent = fixnode_parent;
			}
			if (m_root == node_index_to_index(erased_node))
			{
				m_root = node_index_to_index(fixnode);
			}
			else if (get_node(fixnode_parent).lchild == erased_node_index)
			{
				get_node(fixnode_parent).lchild = fixnode;
			}
			else
			{
				get_node(fixnode_parent).rchild = fixnode;
			}
		}
		else
		{
			get_node(get_node(erased_node).lchild).parent = pnode;
			get_node(pnode).lchild = get_node(erased_node).lchild;
			if (pnode == get_node(erased_node).rchild)
			{
				fixnode_parent = pnode;
			}
			else
			{
				fixnode_parent = get_node(pnode).parent;
				if (fixnode != invalid_node_index)
				{
					get_node(fixnode).parent = fixnode_parent;
				}
				get_node(fixnode_parent).lchild = fixnode;
				get_node(pnode).rchild = get_node(erased_node).rchild;
				get_node(get_node(erased_node).rchild).parent = pnode;
			}
			if (m_root == node_index_to_index(erased_node))
			{
				m_root = node_index_to_index(pnode);
			}
			else if (get_node(get_node(erased_node).parent).lchild == erased_node)
			{
				get_node(get_node(erased_node).parent).lchild = pnode;
			}
			else
			{
				get_node(get_node(erased_node).parent).rchild = pnode;
			}
			get_node(pnode).parent = get_node(erased_node).parent;
		}
	}

	index_t find_insert_node(index_t index, index_t& last_dir) const
	{
		index_t prev = invalid_index;
		index_t current = m_root;
		while (current != invalid_index)
		{
			const node& n = get_node(current);
			prev = current;
			if (index == current)
			{
				return index;
			}
			else if (index < current)
			{
				current = node_index_to_index_check_invalid(n.lchild);
				last_dir = index_t(0);
			}
			else
			{
				current = node_index_to_index_check_invalid(n.rchild);
				last_dir = index_t(1);
			}
		}
		return prev;
	}

	index_t find_min_distance_node(index_t index) const
	{
		index_t current = m_root;
		fhash_size_t min_distance = std::numeric_limits<fhash_size_t>::max();
		index_t min_distance_index = invalid_index;
		while (current != invalid_index)
		{
			const node& n = get_node(current);
			if (index == current)
			{
				return current;
			}

			const fhash_size_t distance = std::abs((index - current).value);
			if (distance < min_distance)
			{
				min_distance_index = current;
				min_distance = distance;
			}

			if (index < current)
			{
				current = node_index_to_index_check_invalid(n.lchild);
			}
			else
			{
				current = node_index_to_index_check_invalid(n.rchild);
			}
		}
		return min_distance_index;
	}

	void add_node(index_t index)
	{
		index_t last_dir;
		index_t insert_index = find_insert_node(index, last_dir);
		if (insert_index == invalid_index)
		{
			assert(m_root == invalid_index);
			m_root = index;
			node& n = get_node(index);
			n.lchild = n.rchild = n.parent = invalid_node_index;
			return;
		}

		node& n = get_node(index);
		n.lchild = n.rchild = invalid_node_index;

		node_index_t& child_index = get_node(insert_index).get_child_index(last_dir);
		assert(child_index == invalid_node_index);
		child_index = index_to_node_index(index);
		n.parent = index_to_node_index(insert_index);
	}

	fhash_size_t allocatable_bucket_size() const
	{
		return m_entries == get_default_entries() ? 0: m_bucket_size_minus_one + 1;
	}

	struct default_entries_initializer
	{
		std::aligned_storage_t<sizeof(entry), alignof(entry)> entries[allocator_policy::min_number_of_entries];
		default_entries_initializer()
		{
			for (int i = 0; i < allocator_policy::min_number_of_entries; i++)
			{
				node& n = get_entries()[i].n;
				n.lchild = n.rchild = n.parent = invalid_node_index;
			}
		}
		entry* get_entries()
		{
			return reinterpret_cast<entry*>(entries);
		}
	};

	static entry* get_default_entries()
	{
		static default_entries_initializer default_entries;
		return default_entries.get_entries();
	}

private:
	entry* m_entries = get_default_entries();
	hasher_t m_hasher;
	fhash_size_t m_entries_size = allocator_policy::min_number_of_entries;
	fhash_size_t m_bucket_size_minus_one = allocator_policy::min_number_of_hash_buckets - 1;
	fhash_size_t m_size = 0;
	index_t m_root = invalid_index;
	index_t m_max_index = invalid_index;
};

template <typename key_t, typename value_t, typename hasher_t, typename allocator_policy>
constexpr typename fhash_table<key_t, value_t, hasher_t, allocator_policy>::index_t 
	fhash_table<key_t, value_t, hasher_t, allocator_policy>::invalid_index;

template <typename key_t, typename value_t, typename hasher_t, typename allocator_policy>
constexpr typename fhash_table<key_t, value_t, hasher_t, allocator_policy>::node_index_t 
	fhash_table<key_t, value_t, hasher_t, allocator_policy>::invalid_node_index;

