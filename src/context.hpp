#pragma once

#include <cz/buffer_array.hpp>
#include <cz/string.hpp>
#include <cz/vector.hpp>
#include <cz/write.hpp>
#include "compiler_error.hpp"
#include "files.hpp"
#include "options.hpp"

namespace red {

struct Context {
    Options options;

    Files files;

    cz::Vector<CompilerError> errors;
    cz::Buffer_Array error_message_buffer_array;

    template <class... Ts>
    void report_error(Span span, Ts... ts) {
        errors.reserve(cz::heap_allocator(), 1);

        CompilerError error;
        CZ_DEBUG_ASSERT(span.start.file == span.end.file);
        error.span = span;

        cz::AllocatedString message;
        message.allocator = error_mesage_buffer_array.allocator();
        // ignore errors in return value
        cz::write(cz::string_writer(&message), ts...);
        message.realloc();
        error.message = /* slice */ message;

        errors.push(error);
    }

    void init() { error_message_buffer_array.create(); }
    void destroy();
};

using C = Context;

}
