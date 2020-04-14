#pragma once

#include <cz/vector.hpp>
#include "token.hpp"

namespace red {
namespace pre {

struct Definition {
    cz::Vector<Token> tokens;

    size_t parameter_len;
    bool is_function;
    bool has_varargs;

    void drop(cz::Allocator);
};

}
}
