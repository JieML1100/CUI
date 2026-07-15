#pragma once
#include <initializer_list>
#include <vector>
#ifndef PROPERTY
#define PROPERTY(t,n) __declspec( property (put = Set##n, get = Get##n)) t n
#define READONLY_PROPERTY(t,n) __declspec( property (get = Get##n) ) t n
#define WRITEONLY_PROPERTY(t,n) __declspec( property (put = Set##n) ) t n
#define GET(t,n) t Get##n() 
#define SET(t,n) void Set##n(t value)
#define PROPERTY_CPP(t,n) __declspec( property (put = Set##n, get = Get##n)); t nt Get##n();t Get##n();
#define GET_CPP(c,t,n) t c::Get##n() 
#define SET_CPP(c,t,n) void c::Set##n(t value)
#define EPROPERTY_R(t,n)READONLY_PROPERTY(t, n);GET(t, n)
#endif 
#ifndef typeof
#define typeof(x) decltype(x)
#endif
#pragma warning(disable: 4267)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
#define USE_STD_VECTOR
#ifdef USE_STD_VECTOR
template<typename T, class _Alloc = std::allocator<T>>
class List : public std::vector<T, _Alloc> {
public:
	List() {}
	List(T* val, int len) {
		this->insert(this->end(), val, val + len);
	}
	List(std::initializer_list<T> val) {
		this->insert(this->end(), val.begin(), val.end());
	}
	List(std::vector< T> val) {
		this->insert(this->end(), val.begin(), val.end());
	}
	~List() {}
	void operator=(std::vector<T>& val) {
		*(std::vector<T>*)this = val;
	}
	void operator=(std::vector<T>* val) {
		*(std::vector<T>*)this = *val;
	}
	T& operator[](int index) {
		return this->data()[index];
	}
	PROPERTY(int, Count);
	GET(int, Count) {
		return this->size();
	}
	SET(int, Count) {
		this->resize(value);
	}
	void Add(T val) {
		this->insert(this->end(), val);
	}
	void Clear() {
		this->clear();
	}
	void AddRange(T* val, int len) {
		this->insert(this->end(), val, val + len);
	}
	void AddRange(std::initializer_list<T> val) {
		this->insert(this->end(), val);
	}
	void AddRange(List<T> val) {
		this->insert(this->end(), val.data(), val.data() + val.size());
	}
	void AddRange(List<T>* val) {
		this->insert(this->end(), val->data(), val->data() + val->size());
	}
	void AddRange(std::vector<T>& val) {
		this->insert(this->end(), val.data(), val.data() + val.size());
	}
	void AddRange(std::vector<T>* val) {
		this->insert(this->end(), val->data(), val->data() + val->size());
	}
	void Insert(int index, T val) {
		if (index >= this->size()) {
			this->push_back(val);
		}
		else if (index >= 0) {
			this->insert(this->begin() + index, val);
		}
	}
	void Insert(int index, std::initializer_list<T> val) {
		if (index >= this->size()) {
			this->AddRange(val);
		}
		else if (index >= 0) {
			this->insert(this->begin() + index, val.begin(), val.end());
		}
	}
	void Insert(int index, std::vector<T>& val) {
		if (index >= this->size()) {
			this->AddRange(val);
		}
		else if (index >= 0) {
			this->insert(this->begin() + index, val.begin(), val.end());
		}
	}
	void Insert(int index, std::vector<T>* val) {
		if (index >= this->size()) {
			this->AddRange(val);
		}
		else if (index >= 0) {
			this->insert(this->begin() + index, val->begin(), val->end());
		}
	}
	void Insert(int index, List<T>& val) {
		if (index >= this->size()) {
			this->AddRange(val);
		}
		else if (index >= 0) {
			this->insert(this->begin() + index, val.begin(), val.end());
		}
	}
	void Insert(int index, List<T>* val) {
		if (index >= this->size()) {
			this->AddRange(val);
		}
		else if (index >= 0) {
			this->insert(this->begin() + index, val->begin(), val->end());
		}
	}
	void RemoveAt(int index) {
		if (index >= 0 && index < this->size()) {
			this->erase(this->begin() + index);
		}
	}
	void RemoveAt(int index, uint32_t num) {
		if (index + num >= this->size()) {
			this->resize(index);
			return;
		}
		memcpy(this->data() + index, this->data() + index + num, num * sizeof(T));
		this->resize(this->size() - num);
	}
	int IndexOf(T value) {
		for (int i = 0; i < this->Count; i++) {
			if (this->at(i) == value) {
				return i;
			}
		}
		return -1;
	}
	bool Contains(T value) {
		return IndexOf(value) >= 0;
	}
	int LastIndexOf(T value) {
		for (int i = this->Count - 1; i >= 0; i--) {
			if (this->at(i) == value) {
				return i;
			}
		}
		return -1;
	}
	int Remove(T item) {
		int num = 0;
		for (int i = this->Count - 1; i >= 0; i--) {
			if (this->data()[i] == item) {
				RemoveAt(i);
				num += 1;
			}
		}
		return num;
	}
	void Swap(int from, int to) {
		std::swap(this->data()[from], this->data()[to]);
	}
	T& First() {
		return this->data()[0];
	}
	T& Last() {
		return this->data()[this->size() - 1];
	}
	void Reverse() {
		std::reverse(this->begin(), this->end());
	}
	std::vector<T>& vector() {
		return *this;
	}
	T* ptr() {
		return this->data();
	}
	T& get(int i) {
		return this->data()[i];
	}
	void set(int i, T val) {
		this->data()[i] = val;
	}
};
#else
template <typename T>
class List {
public:

	typedef T* iterator;
	typedef const T* const_iterator;

	List() : _data(nullptr), _size(0), _capacity(0) {}

	List(const T* val, int len) : _data(nullptr), _size(0), _capacity(0) {
		AddRange(val, len);
	}

	List(std::initializer_list<T> val) : _data(nullptr), _size(0), _capacity(0) {
		AddRange(val);
	}

	List(const List& other) : _data(nullptr), _size(0), _capacity(0) {
		reserve(other._size);
		for (int i = 0; i < other._size; ++i) {
			new (_data + i) T(other._data[i]);
		}
		_size = other._size;
	}

	List(List&& other) noexcept : _data(other._data), _size(other._size), _capacity(other._capacity) {
		other._data = nullptr;
		other._size = 0;
		other._capacity = 0;
	}

	~List() {
		Clear();
		operator delete(_data);
	}

	List& operator=(const List& other) {
		if (this != &other) {
			Clear();
			reserve(other._size);
			for (int i = 0; i < other._size; ++i) {
				new (_data + i) T(other._data[i]);
			}
			_size = other._size;
		}
		return *this;
	}

	List& operator=(List&& other) noexcept {
		if (this != &other) {
			Clear();
			operator delete(_data);
			_data = other._data;
			_size = other._size;
			_capacity = other._capacity;

			other._data = nullptr;
			other._size = 0;
			other._capacity = 0;
		}
		return *this;
	}

	T& operator[](int index) {
		return _data[index];
	}

	const T& operator[](int index) const {
		return _data[index];
	}

	PROPERTY(int, Count);
	GET(int, Count) {
		return static_cast<int>(_size);
	}
	SET(int, Count) {
		resize(value);
	}

	void Add(const T& val) {
		if (_size >= _capacity) {
			reserve(_capacity > 0 ? _capacity * 2 : 1);
		}
		new (_data + _size) T(val);
		++_size;
	}

	void Add(T&& val) {
		if (_size >= _capacity) {
			reserve(_capacity > 0 ? _capacity * 2 : 1);
		}
		new (_data + _size) T(std::move(val));
		++_size;
	}

	void Clear() {
		for (int i = 0; i < _size; ++i) {
			_data[i].~T();
		}
		_size = 0;
	}

	void AddRange(const T* val, int len) {
		reserve(_size + len);
		for (int i = 0; i < len; ++i) {
			new (_data + _size + i) T(val[i]);
		}
		_size += len;
	}

	void AddRange(const std::initializer_list<T>& val) {
		AddRange(val.begin(), val.size());
	}

	void AddRange(const List<T>& val) {
		AddRange(val._data, val._size);
	}

	void AddRange(const List<T>* val) {
		if (val) {
			AddRange(val->_data, val->_size);
		}
	}

	void Insert(int index, const T& val) {
		if (index > _size) {
			index = _size;
		}
		if (_size >= _capacity) {
			reserve(_capacity > 0 ? _capacity * 2 : 1);
		}
		for (int i = _size; i > index; --i) {
			new (_data + i) T(std::move(_data[i - 1]));
			_data[i - 1].~T();
		}
		new (_data + index) T(val);
		++_size;
	}

	void Insert(int index, const std::initializer_list<T>& val) {
		InsertRange(index, val.begin(), val.size());
	}

	void Insert(int index, const List<T>& val) {
		InsertRange(index, val._data, val._size);
	}

	void RemoveAt(int index) {
		if (index < _size) {
			_data[index].~T();
			for (int i = index; i < _size - 1; ++i) {
				new (_data + i) T(std::move(_data[i + 1]));
				_data[i + 1].~T();
			}
			--_size;
		}
	}

	void RemoveAt(int index, int num) {
		if (index < _size) {
			int end = index + num;
			if (end > _size) {
				end = _size;
			}
			for (int i = index; i < end; ++i) {
				_data[i].~T();
			}
			for (int i = end; i < _size; ++i) {
				new (_data + index + i - end) T(std::move(_data[i]));
				_data[i].~T();
			}
			_size -= end - index;
		}
	}

	int IndexOf(const T& value) const {
		for (int i = 0; i < _size; ++i) {
			if (_data[i] == value) {
				return static_cast<int>(i);
			}
		}
		return -1;
	}

	bool Contains(const T& value) const {
		return IndexOf(value) >= 0;
	}

	int LastIndexOf(const T& value) const {
		for (int i = _size; i-- > 0;) {
			if (_data[i] == value) {
				return static_cast<int>(i);
			}
		}
		return -1;
	}

	int Remove(const T& item) {
		int numRemoved = 0;
		for (int i = 0; i < _size;) {
			if (_data[i] == item) {
				RemoveAt(i);
				++numRemoved;
			}
			else {
				++i;
			}
		}
		return numRemoved;
	}

	void Swap(int from, int to) {
		if (from < _size && to < _size) {
			std::swap(_data[from], _data[to]);
		}
	}

	T& First() {
		return _data[0];
	}

	const T& First() const {
		return _data[0];
	}

	T& Last() {
		return _data[_size - 1];
	}

	const T& Last() const {
		return _data[_size - 1];
	}

	void Reverse() {
		for (int i = 0; i < _size / 2; ++i) {
			std::swap(_data[i], _data[_size - 1 - i]);
		}
	}

	T* ptr() {
		return _data;
	}

	const T* ptr() const {
		return _data;
	}

	T& get(int i) {
		return _data[i];
	}

	void set(int i, const T& val) {
		_data[i] = val;
	}


	iterator begin() {
		return _data;
	}

	iterator end() {
		return _data + _size;
	}

	const_iterator begin() const {
		return _data;
	}

	const_iterator end() const {
		return _data + _size;
	}

	const_iterator cbegin() const {
		return _data;
	}

	const_iterator cend() const {
		return _data + _size;
	}

private:
	T* _data;
	int _size;
	int _capacity;

	void reserve(int new_capacity) {
		if (new_capacity > _capacity) {
			T* new_data = static_cast<T*>(operator new(new_capacity * sizeof(T)));
			for (int i = 0; i < _size; ++i) {
				new (new_data + i) T(std::move(_data[i]));
				_data[i].~T();
			}
			operator delete(_data);
			_data = new_data;
			_capacity = new_capacity;
		}
	}

	void resize(int new_size) {
		if (new_size < _size) {
			for (int i = new_size; i < _size; ++i) {
				_data[i].~T();
			}
		}
		else if (new_size > _size) {
			reserve(new_size);
			for (int i = _size; i < new_size; ++i) {
				new (_data + i) T();
			}
		}
		_size = new_size;
	}

	void InsertRange(int index, const T* val, int len) {
		if (index > _size) {
			index = _size;
		}
		reserve(_size + len);

		for (int i = _size; i > index; --i) {
			new (_data + i + len - 1) T(std::move(_data[i - 1]));
			_data[i - 1].~T();
		}

		for (int i = 0; i < len; ++i) {
			new (_data + index + i) T(val[i]);
		}
		_size += len;
	}
};
#endif
#undef USE_STD_VECTOR




template<typename T>
class KeyTable64 {
private:
	KeyTable64<T>** Child;
	T Value;

public:
	KeyTable64() : Child(nullptr), Value(T()) {
	}

	~KeyTable64() {
		if (Child) {
			for (int i = 0; i < 0x100; i++) {
				if (Child[i])
					delete Child[i];
			}
			delete[] Child;
		}
	}

	T Get(uint64_t key) {
		if (!Child)
			return T();

		uint8_t* tmpkey = reinterpret_cast<uint8_t*>(&key);
		KeyTable64<T>* tmp = this;
		for (int i = 0; i < 8; i++, tmpkey++) {
			if (!tmp->Child || !tmp->Child[*tmpkey])
				return T();
			tmp = tmp->Child[*tmpkey];
		}
		return tmp->Value;
	}

	bool Contains(uint64_t key) {
		if (!Child)
			return false;

		uint8_t* tmpkey = reinterpret_cast<uint8_t*>(&key);
		KeyTable64<T>* tmp = this;
		for (int i = 0; i < 8; i++, tmpkey++) {
			if (!tmp->Child || !tmp->Child[*tmpkey])
				return false;
			tmp = tmp->Child[*tmpkey];
		}
		return true;
	}

	void Set(uint64_t key, T value) {
		uint8_t* tmpkey = reinterpret_cast<uint8_t*>(&key);
		KeyTable64<T>* tmp = this;
		for (int i = 0; i < 8; i++, tmpkey++) {
			if (!tmp->Child) {
				tmp->Child = new KeyTable64<T>*[0x100]();
				memset(tmp->Child, 0, sizeof(0x100 * sizeof(KeyTable64<T>*)));
			}
			if (!tmp->Child[*tmpkey]) {
				tmp->Child[*tmpkey] = new KeyTable64<T>();
			}
			tmp = tmp->Child[*tmpkey];
		}
		tmp->Value = value;
	}
};




template<typename T>
class KeyTable32 {
private:
	union {
		KeyTable32<T>** Child;
		T Value;
	};

public:
	KeyTable32() {
		RtlZeroMemory(this, sizeof(*this));
	}

	~KeyTable32() {
		if (Child) {
			for (int i = 0; i < 0x100; i++) {
				if (Child[i])
					delete Child[i];
			}
			delete[] Child;
		}
	}

	T Get(uint32_t key) {
		uint8_t* tmpkey = (uint8_t*)&key;
		KeyTable32<T>* tmp = this;

		if (!tmp->Child || !tmp->Child[tmpkey[3]])
			return T();
		tmp = tmp->Child[tmpkey[3]];

		if (!tmp->Child || !tmp->Child[tmpkey[2]])
			return T();
		tmp = tmp->Child[tmpkey[2]];

		if (!tmp->Child || !tmp->Child[tmpkey[1]])
			return T();
		tmp = tmp->Child[tmpkey[1]];

		if (!tmp->Child || !tmp->Child[tmpkey[0]])
			return T();
		tmp = tmp->Child[tmpkey[0]];

		return tmp->Value;
	}

	bool Contains(uint32_t key) {
		uint8_t* tmpkey = (uint8_t*)&key;
		KeyTable32<T>* tmp = this;

		if (!tmp->Child || !tmp->Child[tmpkey[3]])
			return false;
		tmp = tmp->Child[tmpkey[3]];

		if (!tmp->Child || !tmp->Child[tmpkey[2]])
			return false;
		tmp = tmp->Child[tmpkey[2]];

		if (!tmp->Child || !tmp->Child[tmpkey[1]])
			return false;
		tmp = tmp->Child[tmpkey[1]];

		if (!tmp->Child || !tmp->Child[tmpkey[0]])
			return false;

		return true;
	}

	void Set(uint32_t key, T value) {
		uint8_t* tmpkey = (uint8_t*)&key;
		KeyTable32<T>* tmp = this;

		if (!tmp->Child)
			tmp->Child = new KeyTable32<T>*[0x100]();
		if (!tmp->Child[tmpkey[3]])
			tmp->Child[tmpkey[3]] = new KeyTable32<T>();
		tmp = tmp->Child[tmpkey[3]];

		if (!tmp->Child)
			tmp->Child = new KeyTable32<T>*[0x100]();
		if (!tmp->Child[tmpkey[2]])
			tmp->Child[tmpkey[2]] = new KeyTable32<T>();
		tmp = tmp->Child[tmpkey[2]];

		if (!tmp->Child)
			tmp->Child = new KeyTable32<T>*[0x100]();
		if (!tmp->Child[tmpkey[1]])
			tmp->Child[tmpkey[1]] = new KeyTable32<T>();
		tmp = tmp->Child[tmpkey[1]];

		if (!tmp->Child)
			tmp->Child = new KeyTable32<T>*[0x100]();
		if (!tmp->Child[tmpkey[0]])
			tmp->Child[tmpkey[0]] = new KeyTable32<T>();
		tmp = tmp->Child[tmpkey[0]];

		tmp->Value = value;
	}
};