#pragma once

#include <cz/str.hpp>
#include "span.hpp"

namespace red {

struct Compiler_Error {
    Span span;
    cz::Str message;
};

}
