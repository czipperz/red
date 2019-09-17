#include "hash.hpp"

namespace red {

void hash(Hash* hash, cz::Str key) {
    for (size_t i = 0; i < key.len; ++i) {
        *hash *= 31;
        *hash += key[i];
    }
}

}
