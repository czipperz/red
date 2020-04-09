#include "context.hpp"

namespace red {

void Context::init() {
    error_message_buffer_array.create();
}

void Context::destroy() {
    options.destroy(this);

    error_message_buffer_array.drop();
    errors.drop(cz::heap_allocator());

    files.destroy(allocator);
}

void Context::report_error_str(Span span, cz::Str message) {
    CompilerError error;
    CZ_DEBUG_ASSERT(span.start.file == span.end.file);
    error.span = span;
    error.message = message;

    errors.reserve(cz::heap_allocator(), 1);
    errors.push(error);
}

}
