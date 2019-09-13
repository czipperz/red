#pragma once

#include <cz/context.hpp>
#include "compiler_error.hpp"
#include "files.hpp"
#include "options.hpp"

namespace red {

struct Context : cz::Context {
    const char* program_name;
    Options options;
    Files files;
    cz::Vector<CompilerError> errors;

    template <class... Ts>
    void report_error(size_t file, Location start, Location end, Ts... ts) {
        errors.reserve(allocator, 1);

        CompilerError error;
        error.file = file;
        error.start = start;
        error.end = end;

        cz::Allocated<cz::String> message;
        message.allocator = allocator;
        // ignore errors in return value
        cz::write(cz::string_writer(&message), ts...);
        error.message = message.object;

        errors.push(error);
    }
};

using C = Context;

}
