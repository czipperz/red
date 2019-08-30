#pragma once

#include <cz/context.hpp>
#include "compiler_error.hpp"
#include "options.hpp"

namespace red {

struct Context : cz::Context {
    const char* program_name;
    Options options;
    cz::SmallVector<CompilerError, 0> errors;
};

using C = Context;

}
