#pragma once

#include <cz/string.hpp>
#include "span.hpp"

namespace red {

struct CompilerError {
    Span span;
    cz::String message;
};

}
