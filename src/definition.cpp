#include "definition.hpp"

#include <stdint.h>
#include <cz/string.hpp>
#include <cz/util.hpp>
#include "bit_array.hpp"

namespace red {
namespace cpp {

void Definition::drop(cz::Allocator allocator) {
    tokens.drop(allocator);

    for (size_t j = 0; j < token_values.len(); ++j) {
        token_values[j].drop(allocator);
    }
    token_values.drop(allocator);
}

static void mask_set_present(unsigned char* masks, size_t index) {
    bit_array::set(masks, 2 * index);
    bit_array::unset(masks, 2 * index + 1);
}

static bool mask_is_present(unsigned char* masks, size_t index) {
    return bit_array::get(masks, 2 * index);
}

static void mask_set_tombstone(unsigned char* masks, size_t index) {
    bit_array::unset(masks, 2 * index);
    bit_array::set(masks, 2 * index + 1);
}

static bool mask_is_tombstone(unsigned char* masks, size_t index) {
    return bit_array::get(masks, 2 * index + 1);
}

static size_t mask_alloc_size(size_t cap) {
    return bit_array::alloc_size(2 * cap);
}

void DefinitionMap::reserve(cz::Allocator allocator, size_t extra) {
    if (_cap - _count < extra) {
        size_t new_cap = cz::max(_cap * 2, _count + extra);

        unsigned char* new_masks =
            static_cast<unsigned char*>(allocator.alloc({mask_alloc_size(new_cap), 1}).buffer);
        CZ_ASSERT(new_masks);
        memset(new_masks, 0, mask_alloc_size(new_cap));

        Hash* new_hashes =
            static_cast<Hash*>(allocator.alloc({sizeof(Hash) * new_cap, alignof(Hash)}).buffer);
        CZ_ASSERT(new_hashes != nullptr);

        cz::Str* new_keys = static_cast<cz::Str*>(
            allocator.alloc({sizeof(cz::Str) * new_cap, alignof(cz::Str)}).buffer);
        CZ_ASSERT(new_keys != nullptr);

        Definition* new_values = static_cast<Definition*>(
            allocator.alloc({sizeof(Definition) * new_cap, alignof(Definition)}).buffer);
        CZ_ASSERT(new_values != nullptr);

        DefinitionMap new_map;
        new_map._masks = new_masks;
        new_map._hashes = new_hashes;
        new_map._keys = new_keys;
        new_map._values = new_values;
        new_map._count = _count;
        new_map._cap = new_cap;

        if (_hashes) {
            for (size_t index = 0; index < _cap; ++index) {
                if (mask_is_present(_masks, index)) {
                    cz::String key(const_cast<char*>(_keys[index].buffer), _keys[index].len,
                                   _keys[index].len);
                    new_map.set(key, _hashes[index], allocator, _values[index]);
                }
            }

            allocator.dealloc({_masks, mask_alloc_size(_cap)});
            allocator.dealloc({_hashes, sizeof(Hash) * _cap});
            allocator.dealloc({_keys, sizeof(cz::Str) * _cap});
            allocator.dealloc({_values, sizeof(Definition) * _cap});
        }

        *this = new_map;
    }
}

static Hash hash(cz::Str str) {
    static const Hash hash_salt = 1293486;  // @MagicNumber
    Hash h = hash_salt;
    red::hash(&h, str);
    return h;
}

Definition* DefinitionMap::find(cz::Str key) {
    if (_cap == 0) {
        return nullptr;
    }

    Hash key_hash = hash(key);
    size_t start = key_hash % _cap;
    size_t index = start;

    while (true) {
        if (mask_is_present(_masks, index)) {
            if (_hashes[index] == key_hash) {
                // @Speed: We could remove this check if we don't care about hash collisions.
                if (_keys[index] == key) {
                    return &_values[index];
                }
            }
        } else if (!mask_is_tombstone(_masks, index)) {
            return nullptr;
        }

        ++index;
        if (index >= _cap) {
            index -= _cap;
        }

        // no matches and no space left
        if (index == start) {
            return nullptr;
        }
    }
}

void DefinitionMap::set(cz::String key,
                        Hash key_hash,
                        cz::Allocator allocator,
                        const Definition& definition) {
    if (_cap == 0) {
        CZ_PANIC("DefinitionMap::set(): No space to insert");
    }

    size_t start = key_hash % _cap;
    size_t index = start;

    while (true) {
        if (mask_is_present(_masks, index)) {
            if (_hashes[index] == key_hash) {
                // @Speed: We could remove this check if we don't care about hash collisions.
                if (_keys[index] == key) {
                    key.drop(allocator);
                    _values[index] = definition;
                    return;
                }
            }
        } else {
            mask_set_present(_masks, index);
            _hashes[index] = key_hash;
            _keys[index] = key;
            _values[index] = definition;
            ++_count;
            return;
        }

        ++index;
        if (index >= _cap) {
            index -= _cap;
        }

        // no matches and no space left
        if (index == start) {
            CZ_PANIC("DefinitionMap::set(): No space to insert");
        }
    }
}

void DefinitionMap::set(cz::String key, cz::Allocator allocator, const Definition& value) {
    return set(key, hash(key), allocator, value);
}

void DefinitionMap::remove(cz::Str key, cz::Allocator allocator) {
    Definition* def = find(key);
    if (def) {
        size_t index = def - _values;
        mask_set_tombstone(_masks, index);
        allocator.dealloc({const_cast<char*>(_keys[index].buffer), _keys[index].len});
        def->drop(allocator);
    }
}

void DefinitionMap::drop(cz::Allocator allocator) {
    for (size_t i = 0; i < _cap; ++i) {
        const cz::Str& key = _keys[i];
        if (mask_is_present(_masks, i)) {
            allocator.dealloc({const_cast<char*>(key.buffer), key.len});
        }
    }
    allocator.dealloc({_masks, mask_alloc_size(_cap)});
    allocator.dealloc({_hashes, sizeof(Hash) * _cap});
    allocator.dealloc({_keys, sizeof(cz::Str) * _cap});
    allocator.dealloc({_values, sizeof(Definition) * _cap});
}

Definition* DefinitionMap::get_index(size_t index) {
    if (mask_is_present(_masks, index)) {
        return _values + index;
    } else {
        return nullptr;
    }
}

}
}
