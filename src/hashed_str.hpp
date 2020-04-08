#pragma once

#include <cz/hash.hpp>
#include <cz/str.hpp>

namespace red {

struct Hashed_Str {
    cz::Str str;
    cz::Hash hash;

    bool operator==(const Hashed_Str& other) const { return str.buffer == other.buffer; }
    bool operator!=(const Hashed_Str& other) const { return !(*this == other); }
};

}
