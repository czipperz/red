#pragma once

#include "token.hpp"
#include "span.hpp"

namespace red {

struct Token_Source_Span_Pair {
    Token token;
    Span source_span;
};

}
