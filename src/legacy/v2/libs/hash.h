#ifndef __HASH_H__
#define __HASH_H__

#define switch_hash(str) 						switch (hash(str))
#define case_hash(s)							case hash(s)

inline constexpr unsigned int hash(const char* str) {
    return str && str[0] ? static_cast<unsigned int>(str[0]) + 0xEDB8832Full * hash(str + 1) : 8603;
}

#endif