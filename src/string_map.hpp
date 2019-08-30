#pragma once

#include <stdint.h>
#include <cz/string.hpp>

namespace cz {

using Hash = uint32_t;

void hash(Hash* hash, Str key);

namespace impl {
struct GenericStringMap {
    struct Entry {
        GenericStringMap* _map;
        Str _key;
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

        void insert_unchecked(size_t size, mem::Allocator allocator, const void* value);
    };

    char* _masks = 0;
    Hash* _hashes = 0;
    Str* _keys = 0;
    char* _values = 0;
    size_t _count = 0;
    size_t _cap = 0;

    void reserve(mem::AllocInfo info, mem::Allocator allocator, size_t extra);

    Entry find(Str key);
    Entry find(Str key, Hash key_hash);

    void drop(mem::AllocInfo info, mem::Allocator allocator);
};
}

template <class Value>
class StringMap : impl::GenericStringMap {
    using Generic = impl::GenericStringMap;

public:
    class Entry {
        Generic::Entry entry;
        friend class StringMap;

    public:
        Str key() const { return entry._key; }
        bool is_present() const { return entry._present; }
        bool can_insert() const { return entry._has_space; }

        Value& set(mem::Allocator allocator, const Value& value) {
            if (is_present()) {
                return *and_set(value);
            } else {
                CZ_ASSERT(can_insert());
                return or_insert(allocator, value);
            }
        }

        Value* and_set(const Value& new_value) {
            Value* v = value();
            if (v) {
                *v = new_value;
            }
            return v;
        }

        Value& or_insert(mem::Allocator allocator, const Value& v) {
            if (!can_insert()) {
                CZ_PANIC("StringMap::Entry::or_insert(): No space to insert");
            }

            if (!is_present()) {
                entry.insert_unchecked(sizeof(Value), allocator, &v);
                entry._present = true;
            }

            return *value();
        }

        Value* value() { return static_cast<Value*>(entry.value(sizeof(Value))); }
    };

    void reserve(mem::Allocator allocator, size_t extra) {
        return Generic::reserve(mem::alloc_info<Value>(), allocator, extra);
    }

    Entry find(Str key) {
        Entry entry;
        entry.entry = Generic::find(key);
        return entry;
    }

    void drop(mem::Allocator allocator) {
        return Generic::drop(mem::alloc_info<Value>(), allocator);
    }

    constexpr size_t count() const { return _count; }
    constexpr size_t cap() const { return _cap; }
};

}
