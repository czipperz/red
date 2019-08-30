#pragma once

#include <cz/string.hpp>
#include "location.hpp"

namespace red {

struct CompilerError {
    Location location;
    cz::String message;
};

}
