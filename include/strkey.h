#include <unistd.h>
#include <string>
#include <iostream>

#include "constant.h"

#if !defined(STRKEY_H)
#define STRKEY_H

template <size_t len>
class StrKey {
public:
	//  constexpr 约束保证其定义的是在编译期可求得的值
	static constexpr size_t model_key_size() { return len; }

	// static StrKey max() {
	// 	static StrKey max_key;
	// 	memset(max_key.buf, 255, len);
	// 	return max_key;
	// }
	// static StrKey min() {
	// 	static StrKey min_key;
	// 	memset(min_key.buf, 0, len);
	// 	return min_key;
	// }

	// construct
	StrKey() { memset(buf, '\0', len); }
	StrKey(const char *s) {
		memset(buf, '\0', len);
		memcpy(buf, s, strlen(s));
	}
	StrKey(const std::string &s) {
		memset(buf, '\0', len);
		memcpy(buf, s.data(), s.size());
	}
	StrKey(const StrKey &other) { memcpy(buf, other.buf, len); }
	StrKey &operator=(const StrKey &other) {
		memcpy(buf, &other.buf, len);
		return *this;
	}

	// operation on key
	void set_key(const char *s) {
		memset(buf, '\0', len);
		size_t s_len = strlen(s);
		memcpy(buf, s, s_len);
		buf[s_len] = '\0';
	}

	char *get_name() {
		return buf;
	}

	std::string to_string() const {
		std::string str;
		str.resize(len);
		for (size_t i = 0; i < len; ++i)
		{
			str[i] = buf[i];
		}
		return str;
	}

	void get_model_key(size_t begin_f, size_t l, double *target) const {
		for (size_t i = 0; i < l; i++)
		{
			target[i] = buf[i + begin_f];
		}
	}

	// compare
	// 带起始位置和长度的比较
	bool less_than(const StrKey &other, size_t begin_i, size_t l) const {
		return strncmp(buf + begin_i, other.buf + begin_i, l) < 0;
	}

	friend bool operator<(const StrKey &l, const StrKey &r) {
		return strcmp(l.buf, r.buf) < 0;
	}
	friend bool operator>(const StrKey &l, const StrKey &r) {
		return strcmp(l.buf, r.buf) > 0;
	}
	friend bool operator>=(const StrKey &l, const StrKey &r) {
		return strcmp(l.buf, r.buf) >= 0;
	}
	friend bool operator<=(const StrKey &l, const StrKey &r) {
		return strcmp(l.buf, r.buf) <= 0;
	}
	friend bool operator==(const StrKey &l, const StrKey &r) {
		return strcmp(l.buf, r.buf) == 0;
	}
	friend bool operator!=(const StrKey &l, const StrKey &r) {
		return strcmp(l.buf, r.buf) != 0;
	}

	friend std::ostream &operator<<(std::ostream &os, const StrKey &key) {
		os << key.to_string();
		return os;
	}

	char buf[len];
};

typedef StrKey<MAX_FILE_LEN + 1> index_key_t;

#endif