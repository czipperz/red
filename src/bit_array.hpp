#include <limits.h>
#include <stddef.h>

namespace red {
namespace bit_array {

inline void set(char* array, size_t index) {
    size_t byte = index / CHAR_BIT;
    size_t bit = index % CHAR_BIT;
    char* p = &array[byte];
    *p = *p | (1 << bit);
}

inline void unset(char* array, size_t index) {
    size_t byte = index / CHAR_BIT;
    size_t bit = index % CHAR_BIT;
    char* p = &array[byte];
    *p = *p & ~(1 << bit);
}

inline bool get(const char* array, size_t index) {
    size_t byte = index / CHAR_BIT;
    size_t bit = index % CHAR_BIT;
    const char* p = &array[byte];
    return *p & (1 << bit);
}

inline size_t alloc_size(size_t bits) {
    return (bits + CHAR_BIT - 1) / CHAR_BIT;
}

}
}
