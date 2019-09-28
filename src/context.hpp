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
    void report_error(Span span, Ts... ts) {
        errors.reserve(allocator, 1);

        CompilerError error;
        CZ_DEBUG_ASSERT(span.start.file == span.end.file);
        error.span = span;

        cz::AllocatedString message;
        message.allocator = allocator;
        // ignore errors in return value
        cz::write(cz::string_writer(&message), ts...);
        error.message = /* slice */ message;

        errors.push(error);
    }

    void destroy();
};

using C = Context;

}
