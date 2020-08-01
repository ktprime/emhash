private:
	OwnerPtr owner;
	size_t   index;

public:
	typedef value_type&    reference;
	typedef value_type*    pointer;
	typedef ptrdiff_t difference_type;
	typedef	   size_t		size_type;
	typedef std::bidirectional_iterator_tag iterator_category;

	ClassIterator() : owner(NULL), index(0) {}
	ClassIterator(OwnerPtr o, size_t i) : owner(o), index(i) {}

	reference operator*() const {
		assert(NULL != owner);
		assert(index < owner->end_i());
		return owner->elem_at(index);
	}
	pointer operator->() const {
		assert(NULL != owner);
		assert(index < owner->end_i());
		return &owner->elem_at(index);
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

