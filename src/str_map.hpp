#pragma once

#include <stddef.h>
#include <cz/str.hpp>
#include "hash.hpp"

namespace red {

namespace impl {
struct GenericStrMap {
    struct Entry {
        GenericStrMap* _map;
        cz::Str _key;
        Hash _hash;
        size_t _index;
        bool _present;
        bool _has_space;

        void* value(size_t size) {
            if (_present) {
                return &_map->_values[_index * size];
            } else {
                return nullptr;
            }
        }

        void insert_unchecked(size_t size, const void* value);

        cz::Str found_key() const {
            if (_present) {
                return _map->_keys[_index];
            } else {
                return {nullptr, 0};
            }
        }
    };

    char* _masks = 0;
    Hash* _hashes = 0;
    cz::Str* _keys = 0;
    char* _values = 0;
    size_t _count = 0;
    size_t _cap = 0;

    void reserve(cz::AllocInfo info, cz::Allocator allocator, size_t extra);

    Entry find(cz::Str key, Hash key_hash);

    void drop(cz::AllocInfo info, cz::Allocator allocator);

    void* get_index(cz::AllocInfo info, size_t index);
};
}

template <class Value>
class StrMap : impl::GenericStrMap {
    using Generic = impl::GenericStrMap;

public:
    class Entry {
        Generic::Entry entry;
        friend class StrMap;

    public:
        cz::Str key() const { return entry._key; }
        Hash hash() const { return entry._hash; }
        bool is_present() const { return entry._present; }
        bool can_insert() const { return entry._has_space; }
        size_t index() const { return entry._index; }

        Value& set(const Value& value) {
            if (is_present()) {
                return *and_set(value);
            } else {
                CZ_ASSERT(can_insert());
                return or_insert(value);
            }
        }

        Value* and_set(const Value& new_value) {
            Value* v = value();
            if (v) {
                *v = new_value;
            }
            return v;
        }

        Value& or_insert(const Value& v) {
            if (!is_present()) {
                CZ_ASSERT(can_insert());
                entry.insert_unchecked(sizeof(Value), &v);
                entry._present = true;
            }

            return *value();
        }

        cz::Str found_key() const { return entry.found_key(); }
        Value* value() { return static_cast<Value*>(entry.value(sizeof(Value))); }
    };

    void reserve(cz::Allocator allocator, size_t extra) {
        return Generic::reserve(cz::alloc_info<Value>(), allocator, extra);
    }

    static Hash hash(cz::Str key) {
        static const Hash hash_salt = 1293486;  // @MagicNumber
        Hash key_hash = hash_salt;
        using red::hash;
        hash(&key_hash, key);
        return key_hash;
    }

    Entry find_and_hash(cz::Str key) { return find(key, hash(key)); }

    Entry find(cz::Str key, Hash hash) {
        Entry entry;
        entry.entry = Generic::find(key, hash);
        return entry;
    }

    void drop(cz::Allocator allocator) { return Generic::drop(cz::alloc_info<Value>(), allocator); }

    constexpr size_t count() const { return _count; }
    constexpr size_t cap() const { return _cap; }

    Value* get_index(size_t index) {
        return static_cast<Value*>(Generic::get_index(cz::alloc_info<Value>(), index));
    }
};

}
