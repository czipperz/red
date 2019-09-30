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

    void drop(cz::Allocator);
};

struct DefinitionMap {
    void reserve(cz::Allocator allocator, size_t extra);

    Definition* find(cz::Str key);

    void set(cz::String key, cz::Allocator allocator, const Definition& value);
    void set(cz::String key, Hash key_hash, cz::Allocator allocator, const Definition& value);

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
