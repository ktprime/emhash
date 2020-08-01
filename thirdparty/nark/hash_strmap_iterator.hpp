private:
	OwnerPtr owner;
	size_t   index;

public:
//	reference to reference is not allowed for std::pair in pre-C++0x
//	typedef std::pair<const fstring, Value&> value_type;
	struct value_type {
		typedef const fstring first_type;
		typedef mapped_type   second_type;
		fstring      first;
		mapped_type& second;
		operator std::pair<const fstring, mapped_type>() {
		  return std::pair<const fstring, mapped_type>(first, second);
		}
		value_type(fstring key, mapped_type& val) : first(key), second(val) {}
	};
	typedef ptrdiff_t difference_type;
	typedef	   size_t		size_type;
	typedef std::bidirectional_iterator_tag iterator_category;
	typedef const fstring    key_type;
	typedef value_type      reference;

	// OK for iterator::operator->()
	struct pointer : value_type {
		value_type* operator->() const { return const_cast<pointer*>(this); }
	};

	ClassIterator() : owner(NULL), index(0) {}
	ClassIterator(OwnerPtr o, size_t i) : owner(o), index(i) {}

	reference operator*() const {
		assert(NULL != owner);
		assert(index < owner->end_i());
		return value_type(owner->key(index), owner->val(index));
	}
	pointer operator->() const {
		assert(NULL != owner);
		assert(index < owner->end_i());
		value_type kv(owner->key(index), owner->val(index));
		return static_cast<pointer&>(kv);
	}
	ClassIterator& operator++() {
		assert(index < owner->end_i());
		index = owner->next_i(index);
		return *this;
	}
	ClassIterator& operator--() {
		assert(index < owner->end_i());
		assert(index > 0);
		index = owner->prev_i(index);
		return *this;
	}
	ClassIterator operator++(int) {
		size_t oldindex = index;
		index = owner->next_i(index);
		return ClassIterator(owner, oldindex);
	}
	ClassIterator operator--(int) {
		size_t oldindex = index;
		index = owner->prev_i(index);
		return ClassIterator(owner, oldindex);
	}
	size_t   get_index() const { return index; }
	OwnerPtr get_owner() const { return owner; }

	friend bool operator==(ClassIterator x, ClassIterator y) {
		assert(x.owner == y.owner);
		return x.index == y.index;
	}
	friend bool operator!=(ClassIterator x, ClassIterator y) {
		assert(x.owner == y.owner);
		return x.index != y.index;
	}

#undef ClassIterator

