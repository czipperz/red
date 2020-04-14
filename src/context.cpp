#include "context.hpp"

#include <cz/heap.hpp>

namespace red {

void Context::init() {
    files.init();
    error_message_buffer_array.create();
    temp_buffer_array.create();
}

void Context::destroy() {
    options.destroy();

    temp_buffer_array.drop();

    error_message_buffer_array.drop();
    errors.drop(cz::heap_allocator());
    unspanned_errors.drop(cz::heap_allocator());

    files.destroy();
}

void Context::report_error_str(Span error_span, Span source_span, cz::Str message) {
    Compiler_Error error;
    CZ_DEBUG_ASSERT(error_span.start.file == error_span.end.file);
    error.error_span = error_span;

    CZ_DEBUG_ASSERT(source_span.start.file == source_span.end.file);
    error.source_span = source_span;

    error.message = message;

    errors.reserve(cz::heap_allocator(), 1);
    errors.push(error);
}

void Context::report_error_unspanned(cz::Str message) {
    unspanned_errors.reserve(cz::heap_allocator(), 1);
    unspanned_errors.push(message);
}

}
