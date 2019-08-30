#pragma once

#include <cz/string.hpp>
#include "location.hpp"

namespace red {

struct CompilerError {
    size_t file;
    Location start;
    Location end;
    cz::String message;
};

}
