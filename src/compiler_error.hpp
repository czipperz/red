#pragma once

#include <cz/str.hpp>
#include "span.hpp"

namespace red {

struct Compiler_Error {
    Span error_span;
    Span source_span;
    cz::Str message;
};

}
