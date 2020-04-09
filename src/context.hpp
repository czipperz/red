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

    cz::Vector<Compiler_Error> errors;
    cz::Vector<cz::Str> unspanned_errors;
    cz::Buffer_Array error_message_buffer_array;

    cz::Buffer_Array temp_buffer_array;

    void init();
    void destroy();

    template <class... Ts>
    void report_error(Span span, Ts... ts) {
        cz::AllocatedString message = {};
        message.allocator = error_message_buffer_array.allocator();
        // ignore errors in return value
        cz::write(cz::string_writer(&message), ts...);
        message.realloc();

        report_error_str(span, message);
    }

    void report_error_str(Span span, cz::Str message);

    void report_error_unspanned(cz::Str message);
};

}
