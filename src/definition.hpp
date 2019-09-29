#include <cz/vector.hpp>
#include "token.hpp"

namespace red {
namespace cpp {

struct Definition {
    cz::Vector<Token> tokens;
    cz::Vector<cz::String> token_values;
    cz::Vector<cz::Str> parameters;
    bool is_function;
    bool has_varargs;
};

struct DefinitionMap {
    struct Entry {
        cz::Str key() const { return _key; }
        Hash hash() const { return _hash; }
        bool is_present() const { return _present; }
        bool can_insert() const { return _has_space; }
        size_t index() const { return _index; }

        Definition& set(cz::Allocator allocator, const Definition& value);

        Definition* and_set(const Definition& new_value);

        Definition& or_insert(cz::Allocator allocator, const Definition& v);

        Definition* value();

        DefinitionMap* _map;
        cz::Str _key;
        Hash _hash;
        size_t _index;
        bool _present;
        bool _has_space;
    };

    void reserve(cz::Allocator allocator, size_t extra);

    Entry find(cz::Str key);
    Entry find(cz::Str key, Hash key_hash);

    void drop(cz::Allocator allocator);

    constexpr size_t count() const { return _count; }
    constexpr size_t cap() const { return _cap; }

    Definition* get_index(size_t index);

    unsigned char* _masks = 0;
    Hash* _hashes = 0;
    cz::Str* _keys = 0;
    Definition* _values = 0;
    size_t _count = 0;
    size_t _cap = 0;
};

}
}
