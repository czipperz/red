#include "string_map.hpp"

#include <stdint.h>
#include <cz/util.hpp>
#include "bit_array.hpp"

namespace red {

void hash(Hash* hash, cz::Str key) {
    for (size_t i = 0; i < key.len; ++i) {
        *hash *= 31;
        *hash += key[i];
    }
}

namespace impl {

static void mask_set_present(char* masks, size_t index) {
    bit_array::set(masks, index);
}

static bool mask_is_present(char* masks, size_t index) {
    return bit_array::get(masks, index);
}

static size_t mask_alloc_size(size_t cap) {
    return bit_array::alloc_size(cap);
}

static void insert_unchecked_inner(GenericStringMap::Entry* entry,
                                   size_t size,
                                   cz::Str key,
                                   const void* value) {
    size_t index = entry->_index;
    mask_set_present(entry->_map->_masks, index);
    entry->_map->_hashes[index] = entry->_hash;
    entry->_map->_keys[index] = key;
    memcpy(&entry->_map->_values[index * size], value, size);
}

void GenericStringMap::reserve(cz::AllocInfo info, cz::Allocator allocator, size_t extra) {
    if (_cap - _count < extra) {
        size_t new_cap = cz::max(_cap * 2, _count + extra);

        char* new_masks = static_cast<char*>(allocator.alloc({mask_alloc_size(new_cap), 1}).buffer);
        CZ_ASSERT(new_masks);
        memset(new_masks, 0, mask_alloc_size(new_cap));

        Hash* new_hashes =
            static_cast<Hash*>(allocator.alloc({sizeof(Hash) * new_cap, alignof(Hash)}).buffer);
        CZ_ASSERT(new_hashes != nullptr);

        cz::Str* new_keys = static_cast<cz::Str*>(
            allocator.alloc({sizeof(cz::Str) * new_cap, alignof(cz::Str)}).buffer);
        CZ_ASSERT(new_keys != nullptr);

        char* new_values =
            static_cast<char*>(allocator.alloc({info.size * new_cap, info.alignment}).buffer);
        CZ_ASSERT(new_values != nullptr);

        GenericStringMap new_map;
        new_map._masks = new_masks;
        new_map._hashes = new_hashes;
        new_map._keys = new_keys;
        new_map._values = new_values;
        new_map._count = _count;
        new_map._cap = new_cap;

        if (_hashes) {
            for (size_t index = 0; index < _cap; ++index) {
                if (mask_is_present(_masks, index)) {
                    auto entry = new_map.find(_keys[index], _hashes[index]);
                    CZ_DEBUG_ASSERT(entry._has_space);
                    CZ_DEBUG_ASSERT(!entry._present);
                    insert_unchecked_inner(&entry, info.size, _keys[index],
                                           &_values[index * info.size]);
                }
            }

            allocator.dealloc({_masks, mask_alloc_size(_cap)});
            allocator.dealloc({_hashes, sizeof(Hash) * _cap});
            allocator.dealloc({_keys, sizeof(cz::Str) * _cap});
            allocator.dealloc({_values, info.size * _cap});
        }

        *this = new_map;
    }
}

static GenericStringMap::Entry empty_entry(GenericStringMap* map, cz::Str key, Hash hash) {
    GenericStringMap::Entry entry;
    entry._map = map;
    entry._key = key;
    entry._hash = hash;
    entry._index = map->_cap;
    entry._present = false;
    entry._has_space = false;
    return entry;
}

GenericStringMap::Entry GenericStringMap::find(cz::Str key) {
    if (_cap == 0) {
        return empty_entry(this, key, 0);
    }

    static const Hash hash_salt = 1293486;  // @MagicNumber
    Hash key_hash = hash_salt;
    hash(&key_hash, key);

    return find(key, key_hash);
}

GenericStringMap::Entry GenericStringMap::find(cz::Str key, Hash key_hash) {
    if (_cap == 0) {
        return empty_entry(this, key, key_hash);
    }

    size_t start = key_hash % _cap;
    size_t index = start;

    while (mask_is_present(_masks, index)) {
        if (_hashes[index] == key_hash) {
            // @Speed: We could remove this check if we don't care about hash collisions.
            if (_keys[index] == key) {
                Entry entry;
                entry._map = this;
                entry._key = key;
                entry._hash = key_hash;
                entry._index = index;
                entry._present = true;
                entry._has_space = true;
                return entry;
            }
        }

        ++index;
        if (index >= _cap) {
            index -= _cap;
        }

        // no matches and no space left
        if (index == start) {
            Entry entry;
            entry._map = this;
            entry._key = key;
            entry._hash = key_hash;
            entry._index = _cap;
            entry._present = false;
            entry._has_space = false;
            return entry;
        }
    }

    // since we can't remove, first open position is where we insert
    Entry entry;
    entry._map = this;
    entry._key = key;
    entry._hash = key_hash;
    entry._index = index;
    entry._present = false;
    entry._has_space = true;
    return entry;
}

void GenericStringMap::Entry::insert_unchecked(size_t size,
                                               cz::Allocator allocator,
                                               const void* value) {
    auto key = _key.duplicate(allocator).as_str();
    insert_unchecked_inner(this, size, key, value);
    ++_map->_count;
}

void GenericStringMap::drop(cz::AllocInfo info, cz::Allocator allocator) {
    for (size_t i = 0; i < _cap; ++i) {
        const cz::Str& key = _keys[i];
        if (mask_is_present(_masks, i)) {
            allocator.dealloc({const_cast<char*>(key.buffer), key.len});
        }
    }
    allocator.dealloc({_masks, mask_alloc_size(_cap)});
    allocator.dealloc({_hashes, sizeof(Hash) * _cap});
    allocator.dealloc({_keys, sizeof(cz::Str) * _cap});
    allocator.dealloc({_values, info.size * _cap});
}

void* GenericStringMap::get_index(cz::AllocInfo info, size_t index) {
    if (mask_is_present(_masks, index)) {
        return _values + info.size * index;
    } else {
        return nullptr;
    }
}

}
}
