#pragma once

#include <cz/string.hpp>
#include "location.hpp"

namespace red {

struct CompilerError {
    Location start;
    Location end;
    cz::String message;
};

}
