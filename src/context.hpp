#pragma once

#include <cz/context.hpp>
#include "options.hpp"

namespace red {

struct Context : cz::Context {
    const char* program_name;
    Options options;
};

using C = Context;

}
