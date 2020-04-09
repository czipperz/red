#pragma once

#include <cz/hash.hpp>
#include <cz/str.hpp>

namespace red {

struct Hashed_Str {
    cz::Str str;
    cz::Hash hash;

    static Hashed_Str from_str(cz::Str str) { return {str, hash_str(str)}; }
    static cz::Hash hash_str(cz::Str str) { return cz::hash(str, 0x7521AB297521AB29); }
};

}
